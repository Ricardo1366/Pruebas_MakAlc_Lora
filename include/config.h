#pragma once

#include <Arduino.h>
// Define si el programa se compila en modo "DEPURACIÓN"
#define DEBUG
// -----------------------------------------------------

// Selecciona para que dipositivo vamos a compilar
#define DISP_RICARDO
// #define DISP_JORGE
// -----------------------------------------------------

#define TIEMPO_ESPERA 20000

// Definiciones para el termometro.
#define TMP36_POWER PIN_A2
#define TMP36_DATA PIN_A1
#define TMP36_V_A_0_GRADOS 0.5
#define TMP36_TIEMPO_ENCENDIDO 2
#define INTENTOS_POR_INICIO 3
#define INICIOS_FALLIDOS_MAX 10
#define INICIOS_DESCANSO 60
#define DATOS_CAYENNE 8

