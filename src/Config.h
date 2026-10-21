#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================
// 1. PINOUT (HARDWARE)
// ==========================================

// Atuadores
#define PIN_PWM_CTRL 11
#define LOAD_ON_PIN 12
#define VBAT_ON_PIN 6
#define BAT_CC_ON_PIN A3
#define PIN_BUZZER 4

// Comunicação
#define PIN_RX_SOFT 2 // RX Arduino -> TX Alvo
#define PIN_TX_SOFT 5 // TX Arduino -> RX Alvo
#define PIN_DUT_RST 3 // Controle de Reset do Alvo
#define BAUD_RATE_CCPS 19200
#define BAUD_RATE_DEBUG 9600

// Sensores Analógicos
#define PIN_AD_LOAD_VOUT A0
#define PIN_AD_CCPS_VOUT A1
#define PIN_AD_CCPS_CAP A2
#define PIN_3V3_CCPS A7

// Display I2C
#define LCD_ADDR 0x27
#define LCD_COLS 20
#define LCD_ROWS 4

// ==========================================
// 2. INPUTS (MAPEAMENTO DE BOTÕES)
// ==========================================
// Comente a linha abaixo se for usar na bancada (sem caixa invertida)
#define PAINEL_INVERTIDO

#ifdef PAINEL_INVERTIDO
#define PIN_BTN_UP 10
#define PIN_BTN_DOWN 7
#define PIN_BTN_LEFT 9
#define PIN_BTN_RIGHT 8
#else
#define PIN_BTN_UP 7
#define PIN_BTN_DOWN 10
#define PIN_BTN_LEFT 8
#define PIN_BTN_RIGHT 9
#endif

// ==========================================
// 3. CONSTANTES DE FÍSICA E CALIBRAÇÃO
// ==========================================
const float R1_DIVIDER = 18000.0;
const float R2_DIVIDER = 1000.0;
const float V_REF_AREF = 2.495;
const float FATOR_CONVERSAO = (R1_DIVIDER + R2_DIVIDER) / R2_DIVIDER;

// ==========================================
// 4. PARÂMETROS DE TESTE (Business Logic)
// ==========================================
const float V_TEST_UNDER = 11.4;
const float V_TEST_0_FRESH_BOOT = 11.8;
const float V_TEST_50_FRESH_BOOT = 13;
const float V_TEST_0_25 = 12.1;
const float V_TEST_25_50 = 12.3;
const float V_TEST_50_75 = 13.2;
const float V_TEST_75_100 = 13.9;
const float V_TEST_OVER = 14.4;
const float V_TEST_CRITICAL = 18.0;

// --- CONSTANTES PARA TESTE AMS ---
const float V_AMS_CHECK = 20.0;	 // Tensão de estabilidade
const float V_AMS_STRESS = 25.0; // Tensão de estresse (destrutiva)
// Faixa do 3V3 da placa. O canal tem resolução de ~46mV (divisor 19:1 + AREF 2,495V),
// então a faixa precisa de folga: 3,1V reprovava placas boas lidas em 3,06V.
const float V_3V3_MIN = 3.0;
const float V_3V3_MAX = 3.6;
// Janela (antes do fim dos 4s do CHECK 20V) em que o 3V3 é promediado para a decisão
const unsigned long JANELA_MEDIA_3V3_MS = 1000;
// No HOLD 25V, o 3V3 precisa ficar fora da faixa continuamente por este tempo para reprovar
const unsigned long TEMPO_FORA_FAIXA_3V3_MS = 500;

const float TOLERANCIA_AJUSTE = 0.10;
// Intervalo mínimo entre correções de PWM: dá tempo do filtro/capacitor de saída
// acompanhar o degrau antes da próxima correção (evita overshoot/oscilação).
const unsigned long INTERVALO_AJUSTE_MS = 40;
// Telemetria de diagnóstico pela Serial (9600). Comente para desligar.
#define DEBUG_TELEMETRIA

// Leituras consecutivas dentro da tolerância para considerar a tensão estabilizada.
const uint8_t LEITURAS_ESTAVEIS_AJUSTE = 5;

#endif