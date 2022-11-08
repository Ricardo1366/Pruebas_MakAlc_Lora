#pragma once

#include <Arduino.h>
// Definición de los pines de la placa.
#define TPL5010_DONE PIN_PC0
#define RFM95_DIO0 	 PIN_PC1
#define TPL5010_WAKE PIN_PC2
#define RFM95_DIO1   PIN_PC3
#define RFM95_RST    PIN_PD6
#define RFM95_SS     PIN_PA7
#define RFM95_SCK    PIN_PA6
#define RFM95_MISO   PIN_PA5
#define RFM95_MOSI   PIN_PA4
#define LED_BUILTIN  DD0

// Definición pines para este programa

// Definiciones para el termometro.
#define TMP36_POWER PIN_A2
#define TMP36_DATA  PIN_A1
#define TMP36_V_A_0_GRADOS 0.5
#define TMP36_TIEMPO_ENCENDIDO 1
#define INTENTOS_POR_INICIO 50
#define INICIOS_FALLIDOS_MAX 25
#define INICIOS_DESCANSO 60