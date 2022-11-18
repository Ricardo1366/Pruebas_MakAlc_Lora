#include <Arduino.h>      // Obligatorio en PlatformIO
#include <config.h>       // Parámetros de configuración del programa
#include <dispositivo.h>  // Fichero donde se definen CLAVES_DEVEUI, CLAVES_APPEUI y CLAVES_APPKEY (no se sube a GitHub)
#include <pines.h>        // Definición de pines de la placa.
#include <funciones.h>
#include <lmic.h>         // Libreria encargada de las comunicaciones.
#include <hal\hal.h>      // Complemento de la libreria Lmic. Se descarga automáticamente al descargar Lmic
#include <RocketScream_LowPowerAVRZero.h>   // Libreria para controls del consumo y poner a dormir al micro.
#include <CayenneLPP.h>   // Libreria encargada de formatear los datos para el envío a Cayenne

// Declaración de funciones.
void onEvent(ev_t ev);
void wakeUp(void);
void do_send(osjob_t *j);
void grabaRegistro(int evento);

// void controlEventosRecepcion(void *pUserData, uint8_t port, const uint8_t *pMsg, size_t nMsg);

void lecturaDatos();

unsigned long inicioCuentaAtras;

/*******************************************************
 * Pines del ATmega4808 no utilizados en la aplicación *
 * se ponen aquí para reducir el consumo de corriente. *
 *******************************************************/
#if defined(DEBUG)
// Si estamos en modo debug no deshabilitamos El pin con el led, A3 (para leer la tensión, ni RX2 y TX2 para comunicarnos con el PC)
const uint8_t unusedPins[] = {PIN_A0, PIN_A4, PIN_A5, PIN_A7, PIN_PA1, PIN_PA2, PIN_PA3};
#else
const uint8_t unusedPins[] = {PIN_A0, PIN_A3, PIN_A4, PIN_A5, PIN_A7, PIN_PA0, PIN_PA1, PIN_PA2, PIN_PA3, PIN_PF0, PIN_PF1};
#endif

// ################# DATOS PLACA PARA CONEXIÓN CON HELIUM ##################
// Device EUI. Al copiar los datos de la consola de Helium hay que indicarle que lo haga en formato "lsb"
static const u1_t PROGMEM DEVEUI[8] = {CLAVES_DEVEUI};
void os_getDevEui(u1_t *buf) { memcpy_P(buf, DEVEUI, 8); }

// App EUI. Al copiar los datos de la consola de Helium hay que indicarle que lo haga en formato "lsb"
static const u1_t PROGMEM APPEUI[8] = {CLAVES_APPEUI};
void os_getArtEui(u1_t *buf) { memcpy_P(buf, APPEUI, 8); }

// App Key. Al copiar los datos de la consola de Helium hay que indicarle que lo haga en formato "msb"
static const u1_t PROGMEM APPKEY[16] = {CLAVES_APPKEY};
void os_getDevKey(u1_t *buf) { memcpy_P(buf, APPKEY, 16); }
// ############################################################################

// Sensor de temperatura y humedad.
const double tensionReferencia = 1.5, resolucionLectura = 1023;
double temperatura, lecturaTMP36, tension;

// Reservamos espacio para temperatura.
CayenneLPP datosCayenne(DATOS_CAYENNE);
static osjob_t sendjob;

// Al compilar aparece un aviso de que no tiene el pinmap de la placa y que debemos utilizar un propio.
// Le damos el pin map de la placa.
const lmic_pinmap lmic_pins = {
    .nss = RFM95_SS,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = RFM95_RST,
    .dio = {RFM95_DIO0, RFM95_DIO1, LMIC_UNUSED_PIN},
};

void setup()
{
#if defined(DEBUG)
  EEPROM.begin();
  pinMode(PIN_A3, INPUT);
  pinMode(DD0, OUTPUT);
  size_eeprom = EEPROM.length();
  Serial2.begin(115200);
#endif
  // Iniciamos LMIC.
  os_init();
  // 
  LMIC_reset();

  // Reduce el consumo de los pines que no se van a usar en el ATmega4808
  for (uint8_t index = 0; index < sizeof(unusedPins); index++)
  {
    pinMode(unusedPins[index], INPUT_PULLUP);
    LowPower.disablePinISC(unusedPins[index]);
  }

  // Configuramos la tensión de referencia para la lectura del TMP36
  analogReference(INTERNAL1V5);

  // Configuramos pin de encendido/apagado del TMP36
  pinMode(TMP36_POWER, OUTPUT);

  // Configuramos el pin "Estamos despiertos".
  pinMode(TPL5010_DONE, OUTPUT);

#if defined(DEBUG)
  Serial2.println(F("Antes reiniciar LMIC"));
  delay(5000);
#endif

  LMIC_setClockError(1 * MAX_CLOCK_ERROR / 40);
  /*
  Activar/desactivar la validación de comprobación de enlaces.
  El modo de comprobación de vínculos está habilitado de forma predeterminada y se utiliza
  para verificar periódicamente la conectividad de red.
  Debe llamarse solo si se establece una sesión.
  */
  LMIC_setLinkCheckMode(0);
  LMIC_setDrTxpow(DR_SF7, 14); // 7,14
#if defined(DEBUG)
  Serial2.println(F("LMIC configurada"));
#endif
  // Informamos al temporizador que estamos despiertos.
  wakeUp();

  // Configuramos la interrupción en el ATmega4808 que produce el pin de Wake del TPL5010
  attachInterrupt(digitalPinToInterrupt(TPL5010_WAKE), wakeUp, FALLING);

  // Hacemos la primera lectura y nuestro primer envio de datos.
  nuevaLectura = true;
#if defined(DEBUG)
  lectura = analogRead(PIN_A3);
#endif
}

void loop()
{
#if defined(DEBUG)
  if (lectura < 100)
  {
#endif
    while (iniciosDescanso > 0)
    {
#if defined(DEBUG)
      grabaRegistro(EV_DESCANSO_FORZADO);
#endif
      iniciosDescanso--;
      LowPower.powerDown();
    }

    // Comprobamos si hay que "dormir" el micro.
    if (aDormir && conectados)
    {
      // Trasmisión finalizada y estamos conectados. ¡ A DORMIR !
#if defined(DEBUG)
      grabaRegistro(EV_A_DORMIR);
#endif
      LowPower.powerDown();
      aDormir = false;
      nuevaLectura = true;
    }
    if (aDormir && !conectados)
    {
      // La trasmisión a finalizado, pero todavia no hemos conectado.
      // Esperamos unos segundos antes de dormir.
      if (millis() - cuentaAtras > TIEMPO_ESPERA)
      {
        // Si hemos superado el tiempo de respuesta de "unidos", probamos de nuevo.
        cuentaAtras = millis();
        numeroIntentos++;
        nuevaLectura = true;
      }
      if (numeroIntentos == INTENTOS_POR_INICIO)
      {
        // Hemos llegado al número de intentos de conexión por inicio.
        numeroIntentos = 0;
        iniciosFallidos++;
        aDormir = false;
        if (iniciosFallidos == INICIOS_FALLIDOS_MAX)
        {
          iniciosDescanso = INICIOS_DESCANSO;
        }
        // Contabilizamos un fallo y nos ponemos a dormir.
#if defined(DEBUG)
        grabaRegistro(EV_A_DORMIR);
#endif
        LowPower.powerDown();
        // Al despertar lo intentamos de nuevo.
        aDormir = false;
        nuevaLectura = true;
      }
    }

    // Acabamos de despertar. Comprobamos si hay que hacer un envio.
    if (nuevaLectura)
    {
      lecturaDatos();
      // Lectura hecha.
      nuevaLectura = false;
      // Habilitamos el envío.
      envioDatos = true;
    }

    if (envioDatos)
    {
      // Ponemos en cola el envío de datos.
#if defined(DEBUG)
      Serial2.println(F("Programando envío."));
#endif
      do_send(&sendjob);
      envioDatos = false;
    }
    // Comprueba si tiene que hacer un envío.
    os_runloop_once();
#if defined(DEBUG)
  }
  else
  {
    size_Grabado = 0;
    contador = 0;
    while (size_Grabado + size_registro < size_eeprom)
    {
      EEPROM.get(size_Grabado, registro);
      milisegundos = registro.tiempo % 1000;
      registro.tiempo /= 1000;
      segundos = registro.tiempo % 60;
      registro.tiempo /= 60;
      minutos = registro.tiempo % 60;
      registro.tiempo /= 60;
      hora = registro.tiempo;

      Serial2.print(++contador);
      if (registro.evento < 27)
      {
        Serial2.print(" | ");
        Serial2.printf("%02u:%02u:%02u.%03u", hora, minutos, segundos, milisegundos);
        Serial2.print(" | ");
        Serial2.print(Eventos[registro.evento]);
      }
      Serial2.println();
      // size_Grabado += size_registro;
      size_Grabado += 5;
    }
    finalizo = true;
    while (finalizo)
    {
      /* code */
      delay(0);
    }
  }
#endif
}

// TIC Timer 18k -> 36s, 20k -> 60s.
void wakeUp(void)
{
  digitalWrite(TPL5010_DONE, HIGH);
  // Añadimos un delay para el tiempo de señal en alto de DONE del TPL5010.
  for (char i = 0; i < 5; i++)
  {
  } // Ancho de pulso mínimo 100 ns. 8.5 us con i < 3.
  digitalWrite(TPL5010_DONE, LOW);
}

void onEvent(ev_t ev)
{
#if defined(DEBUG)
  Serial2.print(F("Evento: "));
  Serial2.println(ev);
#endif

  switch (ev)
  {
  case EV_SCAN_TIMEOUT:
#if defined(DEBUG)
    grabaRegistro(EV_SCAN_TIMEOUT);
#endif
    break;
  case EV_BEACON_FOUND:
#if defined(DEBUG)
    grabaRegistro(EV_BEACON_FOUND);
#endif
    break;
  case EV_BEACON_MISSED:
#if defined(DEBUG)
    grabaRegistro(EV_BEACON_MISSED);
#endif
    break;
  case EV_BEACON_TRACKED:
#if defined(DEBUG)
    grabaRegistro(EV_BEACON_TRACKED);
#endif
    break;
  case EV_JOINING:
    // Se ha lanzado una petición de conexión.
    // Controlamos el tiempo que pasa antes de ponernos a dormir.
    conectados = false;
    cuentaAtras = millis();
#if defined(DEBUG)
    grabaRegistro(EV_JOINING);
#endif
    break;
  case EV_JOINED:
    // Unidos con exito.
    numeroIntentos = 0;
    iniciosFallidos = 0;
    conectados = true;
#if defined(DEBUG)
    grabaRegistro(EV_JOINED);
#endif
    u4_t netid = 0;
    devaddr_t devaddr = 0;
    u1_t nwkKey[16];
    u1_t artKey[16];
    LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
    // Deshabilitar la validación de comprobación de enlaces (habilitada automáticamente durante la unión,
    //  pero no es compatible con TTN en este momento).
    LMIC_setLinkCheckMode(0); // 0
    break;
  case EV_RFU1:
#if defined(DEBUG)
    grabaRegistro(EV_RFU1);
#endif
    break;
  case EV_JOIN_FAILED:
#if defined(DEBUG)
    grabaRegistro(EV_JOIN_FAILED);
#endif
    break;
  case EV_REJOIN_FAILED:
#if defined(DEBUG)
    grabaRegistro(EV_REJOIN_FAILED);
#endif
    break;
  case EV_TXCOMPLETE:
#if defined(DEBUG)
    grabaRegistro(EV_TXCOMPLETE);
#endif
    aDormir = true;
    break;
  case EV_LOST_TSYNC:
#if defined(DEBUG)
    grabaRegistro(EV_LOST_TSYNC);
#endif
    break;
  case EV_RESET:
#if defined(DEBUG)
    grabaRegistro(EV_RESET);
#endif
    break;
  case EV_RXCOMPLETE:
    // data received in ping slot
#if defined(DEBUG)
    grabaRegistro(EV_RXCOMPLETE);
#endif
    break;
  case EV_LINK_DEAD:
#if defined(DEBUG)
    grabaRegistro(EV_LINK_DEAD);
#endif
    break;
  case EV_LINK_ALIVE:
#if defined(DEBUG)
    grabaRegistro(EV_LINK_ALIVE);
#endif
    break;
  case EV_TXSTART:
#if defined(DEBUG)
    grabaRegistro(EV_TXSTART);
#endif
    break;
  case EV_TXCANCELED:
#if defined(DEBUG)
    grabaRegistro(EV_TXCANCELED);
#endif
    break;
  case EV_RXSTART:
    /* No utilizar el monitor serial en este evento. */
#if defined(DEBUG)
    grabaRegistro(EV_RXSTART);
#endif
    break;
  case EV_JOIN_TXCOMPLETE:
#if defined(DEBUG)
    grabaRegistro(EV_JOIN_TXCOMPLETE);
#endif
    aDormir = true;
    break;
  default:
#if defined(DEBUG)
    Serial2.println(F("Unknown event"));
    Serial2.println((unsigned)ev);
#endif
    break;
  }
  envioEnCurso = false;
}

void lecturaDatos()
{
  // Encendemos el termómetro.
  digitalWrite(TMP36_POWER, HIGH);

  // Esperamos a que se complete el tiempo de encendido.
  inicioCuentaAtras = millis();
  while (millis() - inicioCuentaAtras < TMP36_TIEMPO_ENCENDIDO)
  {
  }

  // Una ve que ha pasado el tiempo de encendido leemos el valor.
  lecturaTMP36 = analogRead(TMP36_DATA);

  // Una vez leida la temperatura apagamos el termómetro.
  digitalWrite(TMP36_POWER, LOW);

  // Ahora convertimos la lectura a voltios.
  tension = tensionReferencia * lecturaTMP36 / resolucionLectura;

  // Ya tenemos la tensión, ahora la pasamos a grados.
  temperatura = (tension - TMP36_V_A_0_GRADOS) * 100.0;

  // Pasamos los datos a formato Cayenne
  datosCayenne.reset();
  datosCayenne.addAnalogInput(1, temperatura);
}

void do_send(osjob_t *j)
{
#if defined(DEBUG)
  Serial2.println(F("Evento do_send()"));
#endif

   // Check if there is not a current TX/RX job running
  if (!(LMIC.opmode & OP_TXRXPEND))
  {
    // Prepare upstream data transmission at the next possible time.
#if defined(DEBUG)
    grabaRegistro(EV_ENVIO_A_COLA);
#endif
    LMIC_setTxData2(1, datosCayenne.getBuffer(), datosCayenne.getSize(), 0);
    aDormir = true;
  }
  else
  {
#if defined(DEBUG)
    Serial2.println(F("OP_TXRXPEND, not sending"));
#endif
  }
}

#if defined(DEBUG)

// Graba un evento en la EEPROM
void grabaRegistro(int evento)
{
	registro.evento = evento;
	registro.tiempo = millis();
	EEPROM.put(size_Grabado, registro);
	size_Grabado += size_registro;
	//
	if (size_Grabado + size_registro > size_eeprom)
	{
		digitalWrite(DD0, HIGH);
		while (true)
		{
			/* code */
		}
	}
}
#endif
