#pragma once

#include <Arduino.h>
#include <config.h>
#include <lmic.h>                         // Libreria encargada de las comunicaciones.
#include <hal\hal.h>                      // Complemento de la libreria Lmic. Se descarga automáticamente al descargar Lmic

#if defined(DEBUG)
#include <EEPROM.h> // Se encarga de habilitar el acceso a la EEPROM

void grabaRegistro(int evento);
#define EV_A_DORMIR_CONECTADO 21
#define EV_DESCANSO_FORZADO 22
#define EV_NUEVA_LECTURA 23
#define EV_ENVIO_DATOS 24
#define EV_DO_SEND 25
#define EV_ENVIO_A_COLA 26
#define EV_A_DORMIR_NO_CONECTADO 27
#define EV_DESPIERTO 28
#define EV_LECTURADATOS 29

int size_eeprom;
int size_Grabado;

char *Eventos[] = {"NONE",
				   "EV_SCAN_TIMEOUT",
				   "EV_BEACON_FOUND",
				   "EV_BEACON_MISSED",
				   "EV_BEACON_TRACKED",
				   "EV_JOINING",
				   "EV_JOINED",
				   "EV_RFU1",
				   "EV_JOIN_FAILED",
				   "EV_REJOIN_FAILED",
				   "EV_TXCOMPLETE",
				   "EV_LOST_TSYNC",
				   "EV_RESET",
				   "EV_RXCOMPLETE",
				   "EV_LINK_DEAD",
				   "EV_LINK_ALIVE",
				   "EV_SCAN_FOUND",
				   "EV_TXSTART",
				   "EV_TXCANCELED",
				   "EV_RXSTART",
				   "EV_JOIN_TXCOMPLETE",
				   "EV_A_DORMIR_CONECTADO",
				   "EV_DESCANSO_FORZADO",
				   "EV_NUEVA_LECTURA",
				   "EV_ENVIO_DATOS",
				   "EV_DO_SEND()",
				   "EV_ENVIO_A_COLA",
				   "EV_A_DORMIR_NO_CONECTADO",
				   "EV_DESPIERTO",
				   "EV_LECTURADATOS"};

struct Registro
{
	unsigned long tiempo;
	byte evento;
};
Registro registro;
const int size_registro = sizeof(registro);

#endif

// Controla si hay un envío en curso
volatile boolean aDormir, envioDatos;
volatile byte numeroIntentos, iniciosFallidos, iniciosDescanso, contador;
volatile bool conectados, finalizo, nuevaLectura;
volatile unsigned long cuentaAtras;

int lectura, hora, minutos, segundos, milisegundos;

// Declaración de funciones.
void onEvent(ev_t ev);
void wakeUp(void);
void do_send(osjob_t *j);


