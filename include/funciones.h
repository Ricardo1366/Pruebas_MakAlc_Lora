// #pragma once
#include <Arduino.h>
#include <config.h>

#if defined(DEBUG)
void grabaRegistro(int evento);
#define EV_A_DORMIR 21
#define EV_DESCANSO_FORZADO 22
#define EV_NUEVA_LECTURA 23
#define EV_ENVIO_DATOS 24
#define EV_DO_SEND 25
#define EV_ENVIO_A_COLA 26

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
				   "EV_A_DORMIR",
				   "EV_DESCANSO_FORZADO",
				   "EV_NUEVA_LECTURA",
				   "EV_ENVIO_DATOS",
				   "EV_DO_SEND()",
				   "EV_ENVIO_A_COLA"};

struct Registro
{
	unsigned long tiempo;
	byte evento;
};
Registro registro;
const int size_registro = sizeof(registro);

#endif
