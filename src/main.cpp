#include <Arduino.h>
#include <pines.h>
#include <lmic.h>
#include <hal\hal.h>
#include <RocketScream_LowPowerAVRZero.h>
#include <CayenneLPP.h>

#define DEBUG

// Declaración de funciones.
void wakeUp(void);
void do_send(osjob_t *j);

// void controlEventosLMIC(void *pUserData, ev_t ev);
// void controlEventosRecepcion(void *pUserData, uint8_t port, const uint8_t *pMsg, size_t nMsg);
void lecturaDatos();

unsigned long inicioCuentaAtras;

// Controla si hay un envío en curso
volatile boolean envioEnCurso, aDormir, envioDatos;
volatile byte numeroIntentos, iniciosFallidos, iniciosDescanso;
bool nuevaLectura;

/***********************************
 * Pines del ATmega4808 no utilizados en la aplicación
 * se ponen aquí para reducir el consumo de corriente.
 **********************************/
#if defined(DEBUG)
const uint8_t unusedPins[] = {PIN_PD0, PIN_PD3, PIN_PD4, PIN_PD5, PIN_PD7};
#else
const uint8_t unusedPins[] = {PIN_PD0, PIN_PD3, PIN_PD4, PIN_PD5, PIN_PD7, PIN_PF0, PIN_PF1};
#endif

// ################# DATOS PLACA PARA CONEXIÓN CON HELIUM ##################
// Device EUI. Al copiar los datos de la consola de Helium hay que indicarle que lo haga en formato "lsb"
static const u1_t PROGMEM DEVEUI[8] = {0x4D, 0xD9, 0x2D, 0x86, 0xED, 0xF9, 0x81, 0x60};
void os_getDevEui(u1_t *buf) { memcpy_P(buf, DEVEUI, 8); }

// App EUI. Al copiar los datos de la consola de Helium hay que indicarle que lo haga en formato "lsb"
static const u1_t PROGMEM APPEUI[8] = {0x68, 0x45, 0x77, 0xD2, 0x4C, 0xF9, 0x81, 0x60};
void os_getArtEui(u1_t *buf) { memcpy_P(buf, APPEUI, 8); }

// App Key. Al copiar los datos de la consola de Helium hay que indicarle que lo haga en formato "msb"
static const u1_t PROGMEM APPKEY[16] = {0x2D, 0x54, 0xE1, 0x50, 0xAE, 0x38, 0x2E, 0x69, 0x32, 0x26, 0xDE, 0xC1, 0xFB, 0x15, 0xC8, 0x9D};
void os_getDevKey(u1_t *buf) { memcpy_P(buf, APPKEY, 16); }
// ############################################################################

// Sensor de temperatura y humedad.
const double tensionReferencia = 1.5, resolucionLectura = 1023, temperaturaReferencia = 25;
double temperatura, lecturaTMP36, tension;

// Reservamos espacio para temperatura.
// (No hay una variable definida que tenga el tamaño. Hay que mirarlo)
CayenneLPP datosCayenne(4);
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
  Serial2.begin(115200);
  Serial2.println(F("Iniciando"));
#endif
  // Iniciamos LMIC.
#if defined(DEBUG)
  Serial2.println(F("Antes iniciar os LMIC"));
#endif
  os_init();
  // os_init_ex(&lmic_pins);
  LMIC_reset();
#if defined(DEBUG)
  Serial2.println(F("LMIC reiniciada"));
#endif

#if defined(DEBUG)
  Serial2.println(F("Antes de pines desactivados."));
#endif

  // Reduce el consumo de los pines que no se van a usar en el ATmega4808
  for (uint8_t index = 0; index < sizeof(unusedPins); index++)
  {
    pinMode(unusedPins[index], INPUT_PULLUP);
    LowPower.disablePinISC(unusedPins[index]);
  }
#if defined(DEBUG)
  Serial2.println(F("Pines desactivados."));
#endif

  // Configuramos la tensión de referencia para la lectura del TMP36
  analogReference(INTERNAL1V5);

  // Configuramos pin de encendido/apagado del TMP36
  pinMode(TMP36_POWER, OUTPUT);

  // Configuramos el pin "Estamos despiertos".
  pinMode(TPL5010_DONE, OUTPUT);

#if defined(DEBUG)
  Serial2.println(F("Pines configurados"));
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
os_setCallback (&sendjob, do_send); 
  // Informamos al temporizador que estamos despiertos.
  wakeUp();

  // Configuramos la interrupción en el ATmega4808 que produce el pin de Wake del TPL5010
  attachInterrupt(digitalPinToInterrupt(TPL5010_WAKE), wakeUp, FALLING);

  // Hacemos la primera lectura y nuestro primer envio de datos.
  lecturaDatos();
  nuevaLectura = true;
}

void loop()
{
  // put your main code here, to run repeatedly:
  while (iniciosDescanso > 0)
  {
    iniciosDescanso--;
    LowPower.powerDown();
  }

  // Comprobamos si hay que "dormir" el micro.
  if (aDormir)
  {
#if defined(DEBUG)
    Serial2.println(F("Nos vamos a dormir......."));
#endif
    delay(3000);
    LowPower.powerDown();
    nuevaLectura = true;
  }

  // Acabamos de despertar. Comprobamos si hay que hacer un envio.
  if (nuevaLectura)
  {
    lecturaDatos();
#if defined(DEBUG)
    Serial2.print(F("Lectura: "));
    Serial2.println(lecturaTMP36);
    Serial2.print(F("Tensión: "));
    Serial2.println(tension);
    Serial2.print(F("Temperatura: "));
    Serial2.println(temperatura);
    Serial2.println(F("Lectura realizada."));
#endif

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
    delay(1500);
#endif

    // os_setCallback(&sendjob, do_send);
    do_send(&sendjob);
    envioDatos = false;
  }
  // Comprueba si tiene que hacer un envío.
  os_runloop_once();
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
#if defined(DEBUG)
  Serial2.println(F("Estamos despiertos."));
#endif
}

void onEvent(ev_t ev)
{
#if defined(DEBUG)
  Serial2.print(F("Evento: "));
  Serial2.println(ev);
  delay(1500);
#endif

  switch (ev)
  {
  case EV_SCAN_TIMEOUT:
    Serial2.println(F("EV_SCAN_TIMEOUT"));
    break;
  case EV_BEACON_FOUND:
    Serial2.println(F("EV_BEACON_FOUND"));
    break;
  case EV_BEACON_MISSED:
    Serial2.println(F("EV_BEACON_MISSED"));
    break;
  case EV_BEACON_TRACKED:
    Serial2.println(F("EV_BEACON_TRACKED"));
    break;
  case EV_JOINING:
    Serial2.println(F("EV_JOINING"));
    break;
  case EV_JOINED:
    // Unidos con exito.
    numeroIntentos = 0;
    iniciosFallidos = 0;

#if defined(DEBUG)
    Serial2.println(F("EV_JOINED"));
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
    Serial2.println(F("EV_RFU1"));
    break;
  case EV_JOIN_FAILED:
    Serial2.println(F("EV_JOIN_FAILED"));
    numeroIntentos++;
    if (numeroIntentos == INTENTOS_POR_INICIO)
    {
      // Hemos llegado al número de intentos de conexión por inicio.
      numeroIntentos = 0;
      iniciosFallidos++;
      aDormir = true;
      if (iniciosFallidos == INICIOS_FALLIDOS_MAX)
      {
        iniciosDescanso = INICIOS_DESCANSO;
      }
    }
    else
    {
      // Intentamos enviar de nuevo los datos.
      envioDatos = true;
    }
    break;
  case EV_REJOIN_FAILED:
    Serial2.println(F("EV_REJOIN_FAILED"));
    break;
  case EV_TXCOMPLETE:
#if defined(DEBUG)
    Serial2.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
    if (LMIC.txrxFlags & TXRX_ACK)
    {
      Serial2.println(F("Received ack"));
    }
    if (LMIC.dataLen)
    {
      Serial2.println(F("Received "));
      Serial2.println(LMIC.dataLen);
      Serial2.println(F(" bytes of payload"));
    }
#endif
    aDormir = true;
    break;
  case EV_LOST_TSYNC:
    Serial2.println(F("EV_LOST_TSYNC"));
    break;
  case EV_RESET:
    Serial2.println(F("EV_RESET"));
    break;
  case EV_RXCOMPLETE:
    // data received in ping slot
    Serial2.println(F("EV_RXCOMPLETE"));
    break;
  case EV_LINK_DEAD:
    Serial2.println(F("EV_LINK_DEAD"));
    break;
  case EV_LINK_ALIVE:
    Serial2.println(F("EV_LINK_ALIVE"));
    break;
  case EV_TXSTART:
    Serial2.println(F("EV_TXSTART"));
    break;
  case EV_TXCANCELED:
    Serial2.println(F("EV_TXCANCELED"));
    break;
  case EV_RXSTART:
    /* No utilizar el monitor serial en este evento. */
    break;
  case EV_JOIN_TXCOMPLETE:
    Serial2.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
    break;
  default:
    Serial2.println(F("Unknown event"));
    Serial2.println((unsigned)ev);
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
}

void do_send(osjob_t *j)
{
#if defined(DEBUG)
  Serial2.println(F("Evento do_send()"));
  delay(1500);
#endif

  // Check if there is not a current TX/RX job running
  if (!(LMIC.opmode & OP_TXRXPEND))
  {
#if defined(DEBUG)
    Serial2.println(F("Preparando envío a la cola."));
    delay(1500);
#endif
    // Prepare upstream data transmission at the next possible time.
    LMIC_setTxData2(1, datosCayenne.getBuffer(), datosCayenne.getSize() - 1, 0);
#if defined(DEBUG)
    Serial2.println(F("Paquete enviado a la cola."));
#endif
  }
  else
  {
    Serial2.println(F("OP_TXRXPEND, not sending"));
  }
  // Next TX is scheduled after TX_COMPLETE event.
}
