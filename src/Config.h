#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================
// CONFIGURAÇÕES DE HARDWARE (PINOUT)
// ==========================================

// --- Atuadores ---
#define PIN_PWM_CTRL      11
#define LOAD_ON_PIN       12
#define VBAT_ON_PIN       6
#define BAT_CC_ON_PIN     A3
#define PIN_BUZZER        4

// --- Comunicação ---
#define PIN_RX_SOFT       2  // RX do Arduino (Liga no TX da placa alvo)
#define PIN_TX_SOFT       5  // TX do Arduino (Liga no RX da placa alvo)
#define PIN_DUT_RST       3  // Pino de Reset da placa alvo
#define BAUD_RATE_CCPS    19200
#define BAUD_RATE_DEBUG   9600

// --- Sensores Analógicos ---
#define PIN_AD_LOAD_VOUT  A0
#define PIN_AD_CCPS_VOUT  A1
#define PIN_AD_CCPS_CAP   A2
#define PIN_3V3_CCPS      A7

// --- Display LCD ---
#define LCD_ADDR          0x27
#define LCD_COLS          20
#define LCD_ROWS          4

// ==========================================
// MAPA DE BOTÕES (BANCADA VS CAIXA)
// ==========================================
// Descomente abaixo se o painel estiver invertido
#define PAINEL_INVERTIDO 

#ifdef PAINEL_INVERTIDO
    #define PIN_BTN_UP    10
    #define PIN_BTN_DOWN  7
    #define PIN_BTN_LEFT  9  
    #define PIN_BTN_RIGHT 8 
#else
    #define PIN_BTN_UP    7
    #define PIN_BTN_DOWN  10
    #define PIN_BTN_LEFT  8
    #define PIN_BTN_RIGHT 9
#endif

// ==========================================
// CONSTANTES DE FÍSICA E CALIBRAÇÃO
// ==========================================
const float R1_DIVIDER    = 18000.0;
const float R2_DIVIDER    = 1000.0;
const float V_REF_AREF    = 2.495;
const float FATOR_CONVERSAO = (R1_DIVIDER + R2_DIVIDER) / R2_DIVIDER;

// ==========================================
// PARAMETROS DOS TESTES
// ==========================================
const float V_TEST_UNDER    = 11.5;
const float V_TEST_0_25     = 12.2;
const float V_TEST_25_50    = 12.8;
const float V_TEST_50_75    = 13.4;
const float V_TEST_75_100   = 13.8;
const float V_TEST_OVER     = 14.4;
const float V_TEST_CRITICAL = 18.0;

const float TOLERANCIA_AJUSTE = 0.10;

#endif