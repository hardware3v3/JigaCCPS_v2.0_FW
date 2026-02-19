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

// --- HARDWARE CONTROL ---
void JigaHardware::setCarga(bool s) { digitalWrite(LOAD_ON_PIN, s ? HIGH : LOW); }
void JigaHardware::setVbat(bool s) { digitalWrite(VBAT_ON_PIN, s ? HIGH : LOW); }
void JigaHardware::setBateriaCC(bool s) { digitalWrite(BAT_CC_ON_PIN, s ? HIGH : LOW); }

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
	pwmAtual = constrain(valor, 0, 255);
	analogWrite(PIN_PWM_CTRL, pwmAtual);
}
int JigaHardware::getPWM() { return pwmAtual; }

bool JigaHardware::ajustarTensaoAutomatico(float alvo, float atual)
{
	float erro = abs(atual - alvo);
	if (erro <= TOLERANCIA_AJUSTE)
		return true;

	int passo = (erro > 1.5) ? 10 : (erro > 0.5 ? 3 : 1);
	setPWM(pwmAtual + ((atual < alvo) ? -passo : passo));
	return false;
}

// --- COMMS ---
void JigaHardware::enviarBytesComoASCII(uint8_t *dados, size_t tam)
{
	char buf[3];
	Serial.print(F("TX > "));
	for (size_t i = 0; i < tam; i++)
	{
		sprintf(buf, "%02X", dados[i]);
		serialSoft->print(buf);
		Serial.print(buf);
	}
	Serial.println();
}

bool JigaHardware::lerPacoteBinario()
{
	if (serialSoft->available())
	{
		unsigned long tLast = millis();
		tamanhoBufferRaw = 0;
		while ((millis() - tLast) < 100)
		{
			if (serialSoft->available())
			{
				uint8_t b = serialSoft->read();
				if (tamanhoBufferRaw < TAM_BUFFER)
					bufferRaw[tamanhoBufferRaw++] = b;
				tLast = millis();
			}
		}
		return (tamanhoBufferRaw > 0);
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

// --- UI ---
void JigaHardware::limparTela() { lcd.clear(); }

// IMPLEMENTAÇÃO DA FUNÇÃO QUE FALTAVA
void JigaHardware::escreverLinha(int linha, const char *texto)
{
	lcd.setCursor(0, linha);
	lcd.print(texto);
}

void JigaHardware::atualizarParteTela(int l, int c, const char *txt)
{
	lcd.setCursor(c, l);
	lcd.print(txt);
}

// --- SENSORS & INPUTS ---
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
	return {lerADC(PIN_AD_LOAD_VOUT), lerADC(PIN_AD_CCPS_VOUT),
		lerADC(PIN_AD_CCPS_CAP), lerADC(PIN_3V3_CCPS)};
}

Tecla JigaHardware::checarBotao(int pino, Tecla id, EstadoBotao &est)
{
	bool leitura = digitalRead(pino);
	if (leitura != est.estadoFisico)
	{
		est.ultimoTempo = millis();
		est.estadoFisico = leitura;
	}
	if ((millis() - est.ultimoTempo) > 40)
	{
		if (leitura != est.estadoLogico)
		{
			est.estadoLogico = leitura;
			if (est.estadoLogico == LOW)
				return id;
		}
	}
	return NENHUMA;
}

Tecla JigaHardware::lerTeclado()
{
	Tecla t;
	if ((t = checarBotao(PIN_BTN_LEFT, TECLA_ESC, btnLeft)) != NENHUMA)
		return t;
	if ((t = checarBotao(PIN_BTN_RIGHT, TECLA_ENTER, btnRight)) != NENHUMA)
		return t;
	if ((t = checarBotao(PIN_BTN_UP, TECLA_CIMA, btnUp)) != NENHUMA)
		return t;
	if ((t = checarBotao(PIN_BTN_DOWN, TECLA_BAIXO, btnDown)) != NENHUMA)
		return t;
	return NENHUMA;
}

bool JigaHardware::checarResetEmergencia()
{
	return (!digitalRead(PIN_BTN_UP) && !digitalRead(PIN_BTN_DOWN) &&
		!digitalRead(PIN_BTN_LEFT) && !digitalRead(PIN_BTN_RIGHT));
}

// --- AUDIO ---
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
	delay(500);
	digitalWrite(PIN_BUZZER, LOW);
	delay(500);
	digitalWrite(PIN_BUZZER, HIGH);
	delay(500);
	digitalWrite(PIN_BUZZER, LOW);
}