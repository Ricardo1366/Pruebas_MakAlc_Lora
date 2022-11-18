#pragma once

// Define si el programa se compila en modo "DEPURACIÓN"
#define DEBUG
// -----------------------------------------------------

// Selecciona para que dipositivo vamos a compilar
#define DISP_RICARDO
// #define DISP_JORGE
// -----------------------------------------------------

#if defined(DEBUG)
#include <EEPROM.h>       // Se encarga de habilitar el acceso a la EEPROM
#endif

#define TIEMPO_ESPERA 4500

// Definiciones para el termometro.
#define TMP36_POWER PIN_A2
#define TMP36_DATA  PIN_A1
#define TMP36_V_A_0_GRADOS 0.5
#define TMP36_TIEMPO_ENCENDIDO 1
#define INTENTOS_POR_INICIO 3
#define INICIOS_FALLIDOS_MAX 10
#define INICIOS_DESCANSO 60
#define DATOS_CAYENNE 4

// Controla si hay un envío en curso
volatile boolean envioEnCurso, aDormir, envioDatos;
volatile byte numeroIntentos, iniciosFallidos, iniciosDescanso;
bool conectados, finalizo, nuevaLectura;

int size_eeprom;
int size_Grabado;
byte contador;
int lectura, hora, minutos, segundos, milisegundos;
unsigned long cuentaAtras;

