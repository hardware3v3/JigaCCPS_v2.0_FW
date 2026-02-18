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

// --- COMUNICAÇÃO ---
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

// --- INTERFACE (IMPLEMENTAÇÃO DAS SOBRECARGAS) ---

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

// 3. Híbrido A (F, F, RAM, RAM)
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

// 4. Híbrido B (F, RAM, RAM, RAM) - O QUE CONSERTA SEU ERRO ATUAL
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