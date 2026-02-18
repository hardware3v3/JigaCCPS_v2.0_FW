
//////////////////////////////////////////////////

// ARQUIVO: Config.h
// CAMINHO: C:\Users\3v3\Documents\PlatformIO\Projects\JigaCCPS_v2.0_FW\src\Config.h

//////////////////////////////////////////////////

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================
// CONFIGURAÃ‡Ã•ES DE HARDWARE (PINOUT)
// ==========================================

// --- Atuadores ---
#define PIN_PWM_CTRL      11
#define LOAD_ON_PIN       12
#define VBAT_ON_PIN       6
#define BAT_CC_ON_PIN     A3
#define PIN_BUZZER        4

// --- ComunicaÃ§Ã£o ---
#define PIN_RX_SOFT       2  // RX do Arduino (Liga no TX da placa alvo)
#define PIN_TX_SOFT       5  // TX do Arduino (Liga no RX da placa alvo)
#define PIN_DUT_RST       3  // Pino de Reset da placa alvo
#define BAUD_RATE_CCPS    19200
#define BAUD_RATE_DEBUG   9600

// --- Sensores AnalÃ³gicos ---
#define PIN_AD_LOAD_VOUT  A0
#define PIN_AD_CCPS_VOUT  A1
#define PIN_AD_CCPS_CAP   A2
#define PIN_3V3_CCPS      A7

// --- Display LCD ---
#define LCD_ADDR          0x27
#define LCD_COLS          20
#define LCD_ROWS          4

// ==========================================
// MAPA DE BOTÃ•ES (BANCADA VS CAIXA)
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
// CONSTANTES DE FÃSICA E CALIBRAÃ‡ÃƒO
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

//////////////////////////////////////////////////

// ARQUIVO: JigaHardware.cpp
// CAMINHO: C:\Users\3v3\Documents\PlatformIO\Projects\JigaCCPS_v2.0_FW\src\JigaHardware.cpp

//////////////////////////////////////////////////

#include "JigaHardware.h"

JigaHardware::JigaHardware() : lcd(LCD_ADDR, LCD_COLS, LCD_ROWS)
{
	pwmAtual = 255;
	tamanhoBufferRaw = 0;
	serialSoft = new SoftwareSerial(PIN_RX_SOFT, PIN_TX_SOFT);
	btnUp = {HIGH, HIGH, 0};
	btnDown = {HIGH, HIGH, 0};
	btnRight = {HIGH, HIGH, 0};
	btnLeft = {HIGH, HIGH, 0};
}

void JigaHardware::begin()
{
	pinMode(PIN_PWM_CTRL, OUTPUT);
	pinMode(LOAD_ON_PIN, OUTPUT);
	pinMode(VBAT_ON_PIN, OUTPUT);
	pinMode(BAT_CC_ON_PIN, OUTPUT);
	pinMode(PIN_BUZZER, OUTPUT);
	pinMode(PIN_BTN_UP, INPUT_PULLUP);
	pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
	pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
	pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
	digitalWrite(PIN_DUT_RST, LOW);
	pinMode(PIN_DUT_RST, OUTPUT);
	analogReference(EXTERNAL);
	lcd.init();
	lcd.backlight();
	Serial.begin(BAUD_RATE_DEBUG);
	serialSoft->begin(BAUD_RATE_CCPS);
	setPWM(255);
	limparTela();
}

// --- CONTROLE DE HARDWARE ---
void JigaHardware::setCarga(bool estado) { digitalWrite(LOAD_ON_PIN, estado ? HIGH : LOW); }
void JigaHardware::setVbat(bool estado) { digitalWrite(VBAT_ON_PIN, estado ? HIGH : LOW); }
void JigaHardware::setBateriaCC(bool estado) { digitalWrite(BAT_CC_ON_PIN, estado ? HIGH : LOW); }
void JigaHardware::desligarTudo()
{
	setCarga(false);
	setVbat(false);
	setBateriaCC(false);
}
void JigaHardware::controlarAlvo(bool ligado)
{
	if (ligado)
	{
		pinMode(PIN_DUT_RST, INPUT);
		delay(100);
	}
	else
	{
		digitalWrite(PIN_DUT_RST, LOW);
		pinMode(PIN_DUT_RST, OUTPUT);
	}
}
void JigaHardware::setPWM(int valor)
{
	if (valor < 0)
		valor = 0;
	if (valor > 255)
		valor = 255;
	pwmAtual = valor;
	analogWrite(PIN_PWM_CTRL, pwmAtual);
}
int JigaHardware::getPWM() { return pwmAtual; }

bool JigaHardware::ajustarTensaoAutomatico(float alvo, float valorAtual)
{
	float diferenca = valorAtual - alvo;
	float erroAbs = abs(diferenca);
	if (erroAbs <= TOLERANCIA_AJUSTE)
		return true;
	int passo = 1;
	if (erroAbs > 1.5)
		passo = 10;
	else if (erroAbs > 0.5)
		passo = 3;
	if (diferenca < 0)
		setPWM(pwmAtual - passo);
	else
		setPWM(pwmAtual + passo);
	return false;
}

// --- COMUNICAÃ‡ÃƒO ---
void JigaHardware::enviarBytesComoASCII(uint8_t *dados, size_t tamanho)
{
	char bufferHex[3];
	Serial.print("TX ASCII > ");
	for (size_t i = 0; i < tamanho; i++)
	{
		sprintf(bufferHex, "%02X", dados[i]);
		serialSoft->print(bufferHex);
		Serial.print(bufferHex);
	}
	Serial.println();
}

bool JigaHardware::lerPacoteBinario()
{
	if (serialSoft->available() > 0)
	{
		unsigned long tempoUltimoByte = millis();
		tamanhoBufferRaw = 0;
		while ((millis() - tempoUltimoByte) < 100)
		{
			if (serialSoft->available())
			{
				uint8_t b = serialSoft->read();
				if (tamanhoBufferRaw < TAM_BUFFER)
					bufferRaw[tamanhoBufferRaw++] = b;
				tempoUltimoByte = millis();
			}
		}
		if (tamanhoBufferRaw > 0)
			return true;
	}
	return false;
}

uint8_t *JigaHardware::getBufferBytes() { return bufferRaw; }
size_t JigaHardware::getTamanhoBuffer() { return tamanhoBufferRaw; }
void JigaHardware::limparBufferUART()
{
	tamanhoBufferRaw = 0;
	while (serialSoft->available())
		serialSoft->read();
}

// --- INTERFACE (IMPLEMENTAÃ‡ÃƒO DAS SOBRECARGAS) ---

void JigaHardware::limparTela() { lcd.clear(); }

void JigaHardware::escreverLinha(int linha, const char *texto)
{
	lcd.setCursor(0, linha);
	lcd.print(texto);
}

// 1. Tudo RAM
void JigaHardware::mostrarTelaMensagem(const char *l1, const char *l2, const char *l3, const char *l4)
{
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(l1);
	lcd.setCursor(0, 1);
	lcd.print(l2);
	lcd.setCursor(0, 2);
	lcd.print(l3);
	lcd.setCursor(0, 3);
	lcd.print(l4);
}

// 2. Tudo Flash (F())
void JigaHardware::mostrarTelaMensagem(const __FlashStringHelper *l1, const __FlashStringHelper *l2, const __FlashStringHelper *l3, const __FlashStringHelper *l4)
{
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(l1);
	lcd.setCursor(0, 1);
	lcd.print(l2);
	if (l3)
	{
		lcd.setCursor(0, 2);
		lcd.print(l3);
	}
	if (l4)
	{
		lcd.setCursor(0, 3);
		lcd.print(l4);
	}
}

// 3. HÃ­brido A (F, F, RAM, RAM)
void JigaHardware::mostrarTelaMensagem(const __FlashStringHelper *l1, const __FlashStringHelper *l2, const char *l3, const char *l4)
{
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(l1);
	lcd.setCursor(0, 1);
	lcd.print(l2);
	lcd.setCursor(0, 2);
	lcd.print(l3);
	lcd.setCursor(0, 3);
	lcd.print(l4);
}

// 4. HÃ­brido B (F, RAM, RAM, RAM) - O QUE CONSERTA SEU ERRO ATUAL
void JigaHardware::mostrarTelaMensagem(const __FlashStringHelper *l1, const char *l2, const char *l3, const char *l4)
{
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(l1);
	lcd.setCursor(0, 1);
	lcd.print(l2);
	lcd.setCursor(0, 2);
	lcd.print(l3);
	lcd.setCursor(0, 3);
	lcd.print(l4);
}

// --- SENSORES ---
float JigaHardware::lerADC(int pino)
{
	long soma = 0;
	for (int i = 0; i < 5; i++)
	{
		soma += analogRead(pino);
		delay(1);
	}
	return (soma / 5.0) * (V_REF_AREF / 1023.0) * FATOR_CONVERSAO;
}

LeituraSensores JigaHardware::lerTudo()
{
	LeituraSensores d;
	d.vLoad = lerADC(PIN_AD_LOAD_VOUT);
	d.vCCPS = lerADC(PIN_AD_CCPS_VOUT);
	d.vCap = lerADC(PIN_AD_CCPS_CAP);
	d.v3v3 = lerADC(PIN_3V3_CCPS);
	return d;
}

// --- INPUTS ---
Tecla JigaHardware::checarBotaoIndividual(int pino, Tecla teclaId, EstadoBotao &estado)
{
	bool leitura = digitalRead(pino);
	if (leitura != estado.estadoFisicoAnterior)
	{
		estado.ultimoTempo = millis();
		estado.estadoFisicoAnterior = leitura;
	}
	if ((millis() - estado.ultimoTempo) > 40)
	{
		if (leitura != estado.estadoLogico)
		{
			estado.estadoLogico = leitura;
			if (estado.estadoLogico == LOW)
				return teclaId;
		}
	}
	return NENHUMA;
}

Tecla JigaHardware::lerTeclado()
{
	Tecla t;
	if ((t = checarBotaoIndividual(PIN_BTN_LEFT, TECLA_ESC, btnLeft)) != NENHUMA)
		return t;
	if ((t = checarBotaoIndividual(PIN_BTN_RIGHT, TECLA_ENTER, btnRight)) != NENHUMA)
		return t;
	if ((t = checarBotaoIndividual(PIN_BTN_UP, TECLA_CIMA, btnUp)) != NENHUMA)
		return t;
	if ((t = checarBotaoIndividual(PIN_BTN_DOWN, TECLA_BAIXO, btnDown)) != NENHUMA)
		return t;
	return NENHUMA;
}

bool JigaHardware::checarResetEmergencia()
{
	return (digitalRead(PIN_BTN_UP) == LOW && digitalRead(PIN_BTN_DOWN) == LOW &&
		digitalRead(PIN_BTN_LEFT) == LOW && digitalRead(PIN_BTN_RIGHT) == LOW);
}

// --- SOM ---
void JigaHardware::beep()
{
	digitalWrite(PIN_BUZZER, HIGH);
	delay(20);
	digitalWrite(PIN_BUZZER, LOW);
}
void JigaHardware::beepSucesso()
{
	for (int i = 0; i < 3; i++)
	{
		beep();
		delay(50);
	}
}
void JigaHardware::beepErro()
{
	digitalWrite(PIN_BUZZER, HIGH);
	delay(800);
	digitalWrite(PIN_BUZZER, LOW);
}
void JigaHardware::tocarMelodiaVitoria()
{
	beep();
	delay(100);
	beep();
	delay(100);
	beep();
	delay(100);
	digitalWrite(PIN_BUZZER, HIGH);
	delay(1000);
	digitalWrite(PIN_BUZZER, LOW);
}
void JigaHardware::tocarMelodiaDerrota()
{
	digitalWrite(PIN_BUZZER, HIGH);
	delay(800);
	digitalWrite(PIN_BUZZER, LOW);
	delay(500);
	digitalWrite(PIN_BUZZER, HIGH);
	delay(1200);
	digitalWrite(PIN_BUZZER, LOW);
}

void JigaHardware::atualizarParteTela(int linha, int coluna, const char* texto) {
    // Posiciona o cursor no lugar exato (buraco do template)
    lcd.setCursor(coluna, linha);
    lcd.print(texto);
}

//////////////////////////////////////////////////

// ARQUIVO: JigaHardware.h
// CAMINHO: C:\Users\3v3\Documents\PlatformIO\Projects\JigaCCPS_v2.0_FW\src\JigaHardware.h

//////////////////////////////////////////////////

#ifndef JIGAHARDWARE_H
#define JIGAHARDWARE_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include "Config.h"

enum Tecla
{
	NENHUMA,
	TECLA_CIMA,
	TECLA_BAIXO,
	TECLA_ENTER,
	TECLA_ESC
};

struct LeituraSensores
{
	float vLoad;
	float vCCPS;
	float vCap;
	float v3v3;
};

struct EstadoBotao
{
	bool estadoFisicoAnterior;
	bool estadoLogico;
	unsigned long ultimoTempo;
};

class JigaHardware
{
private:
	LiquidCrystal_I2C lcd;
	SoftwareSerial *serialSoft;
	int pwmAtual;
	static const int TAM_BUFFER = 64;
	uint8_t bufferRaw[TAM_BUFFER];
	int tamanhoBufferRaw;
	EstadoBotao btnUp, btnDown, btnRight, btnLeft;
	Tecla checarBotaoIndividual(int pino, Tecla teclaId, EstadoBotao &estado);
	float lerADC(int pino);

public:
	JigaHardware();
	void begin();

	// Atuadores
	void setCarga(bool estado);
	void setVbat(bool estado);
	void setBateriaCC(bool estado);
	void controlarAlvo(bool ligado);
	void desligarTudo();
	void setPWM(int valor);
	int getPWM();
	bool ajustarTensaoAutomatico(float alvo, float valorAtual);

	// ComunicaÃ§Ã£o
	void enviarBytesComoASCII(uint8_t *dados, size_t tamanho);
	bool lerPacoteBinario();
	uint8_t *getBufferBytes();
	size_t getTamanhoBuffer();
	void limparBufferUART();

	// Interface HMI
	Tecla lerTeclado();
	bool checarResetEmergencia();
	void limparTela();
	void escreverLinha(int linha, const char *texto);
	void atualizarParteTela(int linha, int coluna, const char* texto);

	// --- SOBRECARGAS DE MENSAGEM (Para resolver todos os erros de compilaÃ§Ã£o) ---

	// 1. Tudo RAM (Strings normais ou variÃ¡veis)
	void mostrarTelaMensagem(const char *l1, const char *l2, const char *l3 = "", const char *l4 = "");

	// 2. Tudo Flash (Uso de F() em tudo)
	void mostrarTelaMensagem(const __FlashStringHelper *l1, const __FlashStringHelper *l2, const __FlashStringHelper *l3 = NULL, const __FlashStringHelper *l4 = NULL);

	// 3. HÃ­brido A: Titulo e Subtitulo em Flash, resto em RAM (Para menus fixos com valores)
	void mostrarTelaMensagem(const __FlashStringHelper *l1, const __FlashStringHelper *l2, const char *l3, const char *l4);

	// 4. HÃ­brido B: SÃ³ TÃ­tulo em Flash, resto em RAM (O QUE FALTAVA!)
	void mostrarTelaMensagem(const __FlashStringHelper *l1, const char *l2, const char *l3, const char *l4);

	// Audio
	void beep();
	void beepSucesso();
	void beepErro();
	void tocarMelodiaVitoria();
	void tocarMelodiaDerrota();

	// Sensores
	LeituraSensores lerTudo();
};

#endif

//////////////////////////////////////////////////

// ARQUIVO: main.cpp
// CAMINHO: C:\Users\3v3\Documents\PlatformIO\Projects\JigaCCPS_v2.0_FW\src\main.cpp

//////////////////////////////////////////////////

#include <Arduino.h>
#include "JigaHardware.h"
#include "TestController.h"

// 1. Instanciar Hardware
JigaHardware hardware;

// 2. Instanciar LÃ³gica de Teste (Injetando o hardware)
TestController controller(hardware);

void setup() {
    // Inicializa Hardware
    hardware.begin();
    
    // Inicializa LÃ³gica
    controller.begin();
    
    hardware.beep(); // Check sonoro de boot
}

void loop() {
    // O loop apenas roda o "cÃ©rebro" da jiga
    controller.run();
}

//////////////////////////////////////////////////

// ARQUIVO: Protocolo.cpp
// CAMINHO: C:\Users\3v3\Documents\PlatformIO\Projects\JigaCCPS_v2.0_FW\src\Protocolo.cpp

//////////////////////////////////////////////////

#include "Protocolo.h"

// --- AUXILIAR: Converte caractere ASCII ('0'-'F') para valor numÃ©rico (0-15) ---
uint8_t charParaByte(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return 10 + (c - 'A');
	if (c >= 'a' && c <= 'f')
		return 10 + (c - 'a');
	return 0;
}

// --- IMPLEMENTAÃ‡ÃƒO DO CRC (MANTIDA) ---
uint16_t Protocolo::calcularCRC16(const uint8_t *data, size_t length)
{
	uint16_t crc = 0x0000;
	uint16_t polynomial = 0xA001;

	for (size_t i = 0; i < length; i++)
	{
		crc ^= data[i];
		for (uint8_t j = 0; j < 8; j++)
		{
			if (crc & 1)
			{
				crc = (crc >> 1) ^ polynomial;
			}
			else
			{
				crc >>= 1;
			}
		}
	}
	return crc;
}

size_t Protocolo::anexarCRC(uint8_t *buffer, size_t length)
{
	uint16_t crc = calcularCRC16(buffer, length);
	// Little Endian
	buffer[length] = (uint8_t)(crc & 0xFF);
	buffer[length + 1] = (uint8_t)((crc >> 8) & 0xFF);
	return length + 2;
}

bool Protocolo::verificarCRC(uint8_t *buffer, size_t length)
{
	if (length < 3)
		return false;

	uint8_t crcLowRecebido = buffer[length - 2];
	uint8_t crcHighRecebido = buffer[length - 1];
	uint16_t crcRecebido = (crcHighRecebido << 8) | crcLowRecebido;

	uint16_t crcCalculado = calcularCRC16(buffer, length - 2);
	return (crcCalculado == crcRecebido);
}

// --- Interpretador com ConversÃ£o ASCII -> BINÃRIO ---
RespostaInterpretada Protocolo::interpretarPacote(uint8_t *bufferRaw, size_t lengthRaw)
{
	RespostaInterpretada resp;
	resp.crcValido = false;
	resp.comando = 0;
	resp.status = 0;

	// Buffer temporÃ¡rio para os bytes reais convertidos
	uint8_t bufferConvertido[32];
	size_t lenConvertido = 0;

	// 1. CONVERSÃƒO (Parser ASCII Hex -> Byte)
	// Percorre o bufferRaw de 2 em 2 caracteres
	for (size_t i = 0; i < lengthRaw; i += 2)
	{

		// Pula caracteres de quebra de linha (\n, \r) se houver no final
		if (bufferRaw[i] == '\r' || bufferRaw[i] == '\n')
			break;
		if (i + 1 >= lengthRaw)
			break; // Evita ler fora do array se for Ã­mpar

		char charHigh = (char)bufferRaw[i];
		char charLow = (char)bufferRaw[i + 1];

		// Se encontrou quebra de linha no meio, para
		if (charHigh == '\r' || charHigh == '\n')
			break;

		// Junta "0" e "5" -> 0x05
		uint8_t valorByte = (charParaByte(charHigh) << 4) | charParaByte(charLow);

		bufferConvertido[lenConvertido++] = valorByte;
	}

	// 2. VALIDAÃ‡ÃƒO (Agora usamos o bufferConvertido)
	if (lenConvertido < 3)
	{
		return resp; // Muito curto
	}

	if (!verificarCRC(bufferConvertido, lenConvertido))
	{
		return resp; // Erro de CRC
	}

	// Se chegou aqui, o CRC bateu!
	resp.crcValido = true;

	// Extrai dados do buffer limpo
	// RX Real: 05 15 F2 ...
	// Byte 0: Comando (05)
	// Byte 1: Status (15)
	if (lenConvertido >= 2)
	{
		resp.comando = bufferConvertido[0];
		resp.status = bufferConvertido[1];
	}

	return resp;
}

//////////////////////////////////////////////////

// ARQUIVO: Protocolo.h
// CAMINHO: C:\Users\3v3\Documents\PlatformIO\Projects\JigaCCPS_v2.0_FW\src\Protocolo.h

//////////////////////////////////////////////////

#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <Arduino.h>

// Estrutura para facilitar o retorno da anÃ¡lise
struct RespostaInterpretada {
    bool crcValido;
    uint8_t comando;
    uint8_t status;
    float valor; // Se houver
};

class Protocolo {
public:
    // --- UtilitÃ¡rios de CRC (Sua lÃ³gica inserida aqui) ---
    static uint16_t calcularCRC16(const uint8_t* data, size_t length);
    static size_t anexarCRC(uint8_t* buffer, size_t length);
    static bool verificarCRC(uint8_t* buffer, size_t length);
    

    // --- Parser ---
    static RespostaInterpretada interpretarPacote(uint8_t* buffer, size_t length);
};

#endif

//////////////////////////////////////////////////

// ARQUIVO: TestController.cpp
// CAMINHO: C:\Users\3v3\Documents\PlatformIO\Projects\JigaCCPS_v2.0_FW\src\TestController.cpp

//////////////////////////////////////////////////

#include "TestController.h"
#include "Protocolo.h"

TestController::TestController(JigaHardware &hw) : jiga(hw)
{
	estadoAtual = ST_MENU;
	opcaoMenu = 0;
	passoLed = 0;
	flagEntradaEstado = false;
}

void TestController::begin()
{
	// Usa F() em tudo para economizar RAM
	jiga.mostrarTelaMensagem(F("INICIANDO SISTEMA..."), F("Jiga CCPS - v3.0"), F("Carregando..."), F(""));
	delay(1000);
	estadoAtual = ST_MENU;
}

void TestController::run()
{
	// 1. Inputs
	Tecla t = jiga.lerTeclado();
	LeituraSensores sensores = jiga.lerTudo();

	// 2. SeguranÃ§a Global
	if (jiga.checarResetEmergencia())
	{
		resetarSistema();
	}

	// 3. MÃ¡quina de Estados
	switch (estadoAtual)
	{
	case ST_MENU:
		handleMenu(t);
		break;
	case ST_MANUAL:
		handleManual(t, sensores);
		break;
	case ST_INIT_TEST:
		handleInitTest();
		break;
	case ST_COM_UART:
		handleComUart();
		break;
	case ST_POWER_CHECK:
		handlePowerCheck(sensores);
		break;
	case ST_POWER_TEST:
		handlePowerTest(t, sensores);
		break;
	case ST_LED_PREP:
		handleLedPrep(sensores);
		break;
	case ST_LED_TEST:
		handleLedTest(t, sensores);
		break;
	case ST_APROVADO:
		handleAprovado(t);
		break;
	case ST_REPROVADO:
		handleReprovado(t);
		break;
	}
}

// --- IMPLEMENTAÃ‡ÃƒO DOS ESTADOS ---

void TestController::handleMenu(Tecla t)
{
	static int menuAnterior = -1;

	if (opcaoMenu != menuAnterior || t != NENHUMA || !flagEntradaEstado)
	{
		// Aqui usamos strings normais (RAM) ou F() consistente
		// Como o menu muda pouco, podemos usar F() se criarmos a sobrecarga,
		// mas para garantir compatibilidade com char*, vamos usar strings normais aqui
		// ou usar F() em tudo se sua classe JigaHardware suportar.
		// Vamos manter RAM aqui para garantir compatibilidade com as strings condicionais.
		jiga.mostrarTelaMensagem(
		    "--- MENU INICIAL ---",
		    (opcaoMenu == 0) ? "> 1. TESTE AUTO" : "  1. TESTE AUTO",
		    (opcaoMenu == 1) ? "> 2. MODO MANUAL" : "  2. MODO MANUAL",
		    "NAV:UP/DN  ENTER:DIR");
		menuAnterior = opcaoMenu;
		flagEntradaEstado = true;
	}

	if (t == TECLA_CIMA)
	{
		opcaoMenu = 0;
		jiga.beep();
	}
	if (t == TECLA_BAIXO)
	{
		opcaoMenu = 1;
		jiga.beep();
	}
	if (t == TECLA_ENTER)
	{
		jiga.beepSucesso();
		flagEntradaEstado = false;
		estadoAtual = (opcaoMenu == 0) ? ST_INIT_TEST : ST_MANUAL;
	}
}

void TestController::handleManual(Tecla t, LeituraSensores &s)
{
	static bool cargaLigada = false;
	static unsigned long tUpdate = 0;

	if (!flagEntradaEstado)
	{
		jiga.mostrarTelaMensagem(F("MODO MANUAL"), F("Inicializando..."), F(""), F(""));
		jiga.setPWM(150);
		cargaLigada = false;
		flagEntradaEstado = true;
	}

	if (t == TECLA_CIMA)
	{
		jiga.setPWM(jiga.getPWM() - 5);
		jiga.beep();
	}
	if (t == TECLA_BAIXO)
	{
		jiga.setPWM(jiga.getPWM() + 5);
		jiga.beep();
	}
	if (t == TECLA_ENTER)
	{
		cargaLigada = !cargaLigada;
		jiga.setCarga(cargaLigada);
		jiga.beepSucesso();
	}
	if (t == TECLA_ESC)
	{
		jiga.desligarTudo();
		jiga.setPWM(255);
		estadoAtual = ST_MENU;
		flagEntradaEstado = false;
	}

	if (millis() - tUpdate > 200)
	{
		char linhaPWM[21], linhaVout[21], linhaCap[21];
		sprintf(linhaPWM, "MANUAL PWM:%d", jiga.getPWM());
		dtostrf(s.vLoad, 4, 1, linhaVout);
		dtostrf(s.vCap, 4, 1, linhaCap);

		char msgVout[21], msgCap[21];
		sprintf(msgVout, "V_OUT: %sV %s", linhaVout, cargaLigada ? "ON " : "OFF");
		sprintf(msgCap, "V_CAP: %sV", linhaCap);

		// Aqui sÃ£o variÃ¡veis RAM (buffers), entÃ£o OK usar a funÃ§Ã£o padrÃ£o
		jiga.mostrarTelaMensagem(linhaPWM, msgVout, msgCap, "ESC p/ Sair");
		tUpdate = millis();
	}
}

void TestController::handleInitTest()
{
	jiga.desligarTudo();
	jiga.setPWM(150);
	jiga.mostrarTelaMensagem(F("INICIANDO TESTES..."), F("Aguarde..."), F(""), F(""));
	delay(1000);

	flagEntradaEstado = false;
	estadoAtual = ST_COM_UART;
}

void TestController::handleComUart()
{
	static bool enviado = false;

	if (!flagEntradaEstado)
	{
		jiga.limparTela();
		jiga.mostrarTelaMensagem(F("1. TESTE SERIAL"), F("Conectando..."), F("Binario + CRC"), F(""));

		jiga.controlarAlvo(true);
		jiga.desligarTudo();
		jiga.setVbat(true);
		jiga.setCarga(false);
		jiga.limparBufferUART();

		timerEstado = millis();
		enviado = false;
		flagEntradaEstado = true;
	}

	// 2. ENVIO
	if (!enviado && millis() - timerEstado > 1000)
	{
		uint8_t pacote[10];
		size_t len = 0;

		pacote[0] = 0x04;
		pacote[1] = 0x81;
		len = 2;

		len = Protocolo::anexarCRC(pacote, len);
		jiga.enviarBytesComoASCII(pacote, len);

		enviado = true;
		timerEstado = millis();
	}

	// 3. RECEPÃ‡ÃƒO
	if (enviado)
	{
		if (jiga.lerPacoteBinario())
		{
			uint8_t *bufRaw = jiga.getBufferBytes();
			size_t tamRaw = jiga.getTamanhoBuffer();

			RespostaInterpretada resp = Protocolo::interpretarPacote(bufRaw, tamRaw);

			if (resp.crcValido)
			{
				uint8_t dados[32];
				int idx = 0;

				// FunÃ§Ã£o lambda definida antes do loop
				auto hex2bin = [](char c) -> uint8_t
				{
					if (c >= '0' && c <= '9')
						return c - '0';
					if (c >= 'A' && c <= 'F')
						return 10 + c - 'A';
					return 0;
				};

				for (size_t i = 0; i < tamRaw; i += 2)
				{
					if (i + 1 >= tamRaw)
						break;
					if (idx >= 32)
						break; // Trava de seguranÃ§a

					char h = bufRaw[i];
					char l = bufRaw[i + 1];
					dados[idx++] = (hex2bin(h) << 4) | hex2bin(l);
				}

				// ExtraÃ§Ã£o dos dados
				uint8_t fwVersion = dados[2];
				uint16_t tensaoFonteRaw = (dados[7] << 8) | dados[8];
				uint16_t tensaoBatRaw = (dados[9] << 8) | dados[10];

				Serial.println(F("=== ANALISE DO PACOTE ==="));
				Serial.print(F("FW: 0x"));
				Serial.println(fwVersion, HEX);
				Serial.print(F("Fonte: "));
				Serial.println(tensaoFonteRaw);
				Serial.print(F("Bat:   "));
				Serial.println(tensaoBatRaw);

				// --- AQUI ESTÃ A ALTERAÃ‡ÃƒO PARA MOSTRAR TENSÃ•ES ---

				// Se quiser aceitar OUTRAS versÃµes, mude para: if (fwVersion >= 0x04 ...)
				if (fwVersion == 0x04 && (resp.status == 0x81 || resp.status == 0x15))
				{
					char msgFonte[21];
					char msgBat[21];

					// Formata para exibir (Ex: "V.Fonte: 1250")
					sprintf(msgFonte, "V.Fonte: %d", tensaoFonteRaw);
					sprintf(msgBat, "V.Bat:   %d", tensaoBatRaw);

					// Exibe tudo no LCD usando a funÃ§Ã£o HÃ­brida (Flash, Flash, RAM, RAM)
					jiga.mostrarTelaMensagem(F("PLACA COMPATIVEL"), F("FW: v04 Detectado"), msgFonte, msgBat);

					jiga.beepSucesso();
					delay(3000); // Aumentei para 3s para dar tempo de ler os valores

					flagEntradaEstado = false;
					estadoAtual = ST_POWER_CHECK;
				}
				else
				{
					char msgErro[21];
					sprintf(msgErro, "Lido: v%02X", fwVersion);

					jiga.mostrarTelaMensagem(F("ERRO VERSAO FW"), F("Esperado: v04"), msgErro, "Falha Serial");
					jiga.beepErro();
					delay(3000);
					estadoAtual = ST_REPROVADO;
				}
			}
			else
			{
				Serial.println(F("ERRO: CRC INVALIDO"));
				jiga.mostrarTelaMensagem(F("ERRO CRC"), F("Dados Corrompidos"), F(""), F(""));
				jiga.beepErro();
				estadoAtual = ST_REPROVADO;
			}
		}

		if (millis() - timerEstado > 5000)
		{
			jiga.mostrarTelaMensagem(F("TIMEOUT"), F("Sem resposta"), F(""), F(""));
			jiga.beepErro();
			flagEntradaEstado = false;
			estadoAtual = ST_REPROVADO;
		}
	}
}

void TestController::handlePowerCheck(LeituraSensores &s)
{
	if (!flagEntradaEstado)
	{
		timerEstado = millis();
		flagEntradaEstado = true;
	}

	char lido[10];
	dtostrf(s.vLoad, 4, 1, lido);
	char msg[21];
	sprintf(msg, "LIDO: %s V", lido);

	// Aqui misturamos Flash com RAM, usamos a versÃ£o HÃ­brida
	jiga.mostrarTelaMensagem(F("2. POWER CHECK"), F("Alvo: 12.0V"), msg, "Ajustando...");

	if (jiga.ajustarTensaoAutomatico(12.0, s.vLoad))
	{
		jiga.beep();
		jiga.setCarga(true);
		flagEntradaEstado = false;
		estadoAtual = ST_POWER_TEST;
	}

	if (millis() - timerEstado > 15000)
	{
		jiga.mostrarTelaMensagem(F("ERRO: TIMEOUT"), F("Fonte instavel"), F(""), F(""));
		estadoAtual = ST_REPROVADO;
	}
	delay(20);
}

void TestController::handlePowerTest(Tecla t, LeituraSensores &s)
{
	// Timer para nÃ£o atualizar o nÃºmero rÃ¡pido demais (ilegÃ­vel)
	static unsigned long tUpdateDisplay = 0;

	// --- 1. DESENHA O TEMPLATE (SÃ³ uma vez ao entrar) ---
	if (!flagEntradaEstado)
	{
		timerEstado = millis();
		tUpdateDisplay = 0;

		jiga.limparTela();
		// Desenhamos tudo o que Ã© FIXO agora.
		// Note os espaÃ§os em branco na linha 2 para encaixar o nÃºmero depois
		// Linha 0: "2. ALIMENTACAO"
		// Linha 1: "LED POWER ACESO?"
		// Linha 2: "VCap:      V"  <-- O nÃºmero vai entrar depois do "VCap: "
		// Linha 3: "DIR >> CONFIRMA"
		jiga.mostrarTelaMensagem(
		    F("2. ALIMENTACAO"),
		    F("LED POWER ACESO?"),
		    F("VCap:      V"),
		    F("DIR >> CONFIRMA"));

		flagEntradaEstado = true;
	}

	// --- 2. ATUALIZA SÃ“ O NÃšMERO (A cada 200ms) ---
	if (millis() - tUpdateDisplay > 200)
	{
		char vCapStr[10];
		// Formata o float. 4 digitos total, 1 decimal (ex: "12.5")
		dtostrf(s.vCap, 4, 1, vCapStr);

		// Escreve APENAS o nÃºmero na Linha 2, Coluna 6 (Logo apÃ³s "VCap: ")
		jiga.atualizarParteTela(2, 6, vCapStr);

		tUpdateDisplay = millis();
	}

	// --- 3. LÃ“GICA DE TESTE (Roda na velocidade mÃ¡xima) ---

	// ValidaÃ§Ã£o AutomÃ¡tica do Capacitor (Monitoramento contÃ­nuo)
	if (s.vCap < 5.0 && (millis() - timerEstado > 2000))
	{
		jiga.setCarga(false);
		jiga.mostrarTelaMensagem(F("FALHA: CAPACITOR"), F("Tensao baixa"), F(""), F(""));
		jiga.beepErro();
		estadoAtual = ST_REPROVADO;
	}

	// ValidaÃ§Ã£o Visual pelo Operador
	if (t == TECLA_ENTER)
	{
		jiga.beep();
		jiga.setCarga(false);
		passoLed = 0;
		flagEntradaEstado = false;
		estadoAtual = ST_LED_PREP;
	}
}

void TestController::handleLedPrep(LeituraSensores &s)
{
	jiga.setVbat(true);

	char alvoStr[10];
	dtostrf(listaTensoes[passoLed], 4, 1, alvoStr);
	char msg[21];
	sprintf(msg, "Alvo: %sV", alvoStr);

	jiga.mostrarTelaMensagem(F("3. AJUSTE LEDS"), msg, "Aguarde...", "");

	if (jiga.ajustarTensaoAutomatico(listaTensoes[passoLed], s.vLoad))
	{
		flagEntradaEstado = false;
		estadoAtual = ST_LED_TEST;
	}
	delay(15);
}

void TestController::handleLedTest(Tecla t, LeituraSensores &s)
{
    // Controle de tempo para atualizaÃ§Ã£o do display
    static unsigned long tUpdateDisplay = 0;
    // Precisamos saber se o passo mudou para redesenhar a instruÃ§Ã£o (Ex: "VM PISCA" -> "VM ACESO")
    static int passoAnterior = -1;

    // --- 1. CONTROLE DE TENSÃƒO (Prioridade MÃ¡xima - Roda sempre) ---
    // MantÃ©m o ajuste fino rodando a cada milissegundo para a tensÃ£o nÃ£o cair
    jiga.ajustarTensaoAutomatico(listaTensoes[passoLed], s.vLoad);

    // --- 2. DESENHO DO TEMPLATE (SÃ³ se mudou o passo ou entrou no estado) ---
    if (!flagEntradaEstado || passoLed != passoAnterior)
    {
        jiga.limparTela();
        
        // Desenhamos a estrutura fixa.
        // Linha 0: "3. TESTE LEDS"
        // Linha 1: VariÃ¡vel (Ex: "VM PISCA (0-25)?")
        // Linha 2: "V-JIGA:      V" (Deixamos espaÃ§o para o nÃºmero)
        // Linha 3: "DIR >> CONFIRMA"
        
        // Nota: listaMsgs[] Ã© um array de char*, entÃ£o combina com a funÃ§Ã£o hÃ­brida (F, RAM, RAM, RAM)
        jiga.mostrarTelaMensagem(
            F("3. TESTE LEDS"), 
            listaMsgs[passoLed], 
            "V-JIGA:      V", 
            "DIR >> CONFIRMA"
        );

        passoAnterior = passoLed;
        flagEntradaEstado = true;
        tUpdateDisplay = 0; // ForÃ§a atualizaÃ§Ã£o imediata do nÃºmero
    }

    // --- 3. ATUALIZAÃ‡ÃƒO DO VALOR (A cada 200ms) ---
    if (millis() - tUpdateDisplay > 200)
    {
        char atualStr[10];
        dtostrf(s.vLoad, 4, 2, atualStr); // Formata com 2 casas decimais
        
        // Escreve apenas o nÃºmero na linha 2, coluna 8 (Logo apÃ³s "V-JIGA: ")
        jiga.atualizarParteTela(2, 8, atualStr);
        
        tUpdateDisplay = millis();
    }

    // --- 4. INTERAÃ‡ÃƒO ---
    if (t == TECLA_ENTER)
    {
        jiga.beep();
        passoLed++;
        
        if (passoLed >= TOTAL_PASSOS_LED)
        {
            flagEntradaEstado = false; // Reset para o prÃ³ximo estado
            estadoAtual = ST_APROVADO;
        }
        else
        {
            // NÃ£o mudamos o estadoAtual (continuamos no ST_LED_TEST),
            // mas como 'passoLed' mudou, o 'if' do passo 2 vai rodar e atualizar a tela.
            // Apenas damos um pequeno delay para evitar "bounce" visual
            delay(200); 
        }
    }
}


void TestController::handleAprovado(Tecla t)
{
	if (!flagEntradaEstado)
	{
		jiga.desligarTudo();
		jiga.setPWM(255);
		jiga.mostrarTelaMensagem(F("   >>> APROVADO <<< "), F("  Placa OK! Remova. "), F("ENTER: Novo Teste"), F("ESC:   Menu Inicial"));
		jiga.tocarMelodiaVitoria();
		flagEntradaEstado = true;
	}

	if (t == TECLA_ENTER)
	{
		jiga.beepSucesso();
		flagEntradaEstado = false;
		estadoAtual = ST_INIT_TEST;
	}
	if (t == TECLA_ESC)
	{
		jiga.beep();
		flagEntradaEstado = false;
		estadoAtual = ST_MENU;
	}
}

void TestController::handleReprovado(Tecla t)
{
	if (!flagEntradaEstado)
	{
		jiga.desligarTudo();
		jiga.setPWM(255);
		jiga.mostrarTelaMensagem(F("   >>> FALHA <<<   "), F("Tente Novamente"), F("PRESSIONE QUALQUER"), F("BOTAO PARA SAIR"));
		jiga.tocarMelodiaDerrota();
		flagEntradaEstado = true;
	}

	if (t != NENHUMA)
	{
		jiga.beep();
		flagEntradaEstado = false;
		estadoAtual = ST_INIT_TEST;
	}
}

void TestController::resetarSistema()
{
	jiga.limparTela();
	jiga.mostrarTelaMensagem(F("   RESET GLOBAL   "), F("Reiniciando..."), F(""), F(""));
	delay(1000);
	void (*resetarArduino)(void) = 0;
	resetarArduino();
}

//////////////////////////////////////////////////

// ARQUIVO: TestController.h
// CAMINHO: C:\Users\3v3\Documents\PlatformIO\Projects\JigaCCPS_v2.0_FW\src\TestController.h

//////////////////////////////////////////////////

#ifndef TESTCONTROLLER_H
#define TESTCONTROLLER_H

#include "JigaHardware.h"

enum EstadoSistema
{
	ST_MENU,
	ST_MANUAL,
	ST_INIT_TEST,
	ST_COM_UART,
	ST_POWER_CHECK,
	ST_POWER_TEST,
	ST_LED_PREP,
	ST_LED_TEST,
	ST_APROVADO,
	ST_REPROVADO
};

class TestController
{
private:
	JigaHardware &jiga;
	EstadoSistema estadoAtual;

	// VariÃ¡veis de fluxo
	unsigned long timerEstado;
	int opcaoMenu;
	int passoLed;
	bool flagEntradaEstado;

	// Tabelas de Teste
	static const int TOTAL_PASSOS_LED = 7;
	const float listaTensoes[7] = {V_TEST_UNDER, V_TEST_0_25, V_TEST_25_50, V_TEST_50_75, V_TEST_75_100, V_TEST_OVER, V_TEST_CRITICAL};
	const char *listaMsgs[7] = {"LEDS APAGADOS?", "VM PISCA (0-25)?", "VM ACESO (25-50)?", "VD PISCA (50-75)?", "VD ACESO (100%)?", "PISCA ALTERNADO?", "PISCA JUNTOS?"};

	// MÃ©todos Privados para cada Estado
	void handleMenu(Tecla t);
	void handleManual(Tecla t, LeituraSensores &s);
	void handleInitTest();
	void handleComUart();
	void handlePowerCheck(LeituraSensores &s);
	void handlePowerTest(Tecla t, LeituraSensores &s);
	void handleLedPrep(LeituraSensores &s);
	void handleLedTest(Tecla t, LeituraSensores &s);
	void handleAprovado(Tecla t);
	void handleReprovado(Tecla t);

	// UtilitÃ¡rios
	void resetarSistema();

public:
	TestController(JigaHardware &hw);
	void begin();
	void run(); // Chama isso no loop
};

#endif
