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

	// Comunicação
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

	// --- SOBRECARGAS DE MENSAGEM (Para resolver todos os erros de compilação) ---

	// 1. Tudo RAM (Strings normais ou variáveis)
	void mostrarTelaMensagem(const char *l1, const char *l2, const char *l3 = "", const char *l4 = "");

	// 2. Tudo Flash (Uso de F() em tudo)
	void mostrarTelaMensagem(const __FlashStringHelper *l1, const __FlashStringHelper *l2, const __FlashStringHelper *l3 = NULL, const __FlashStringHelper *l4 = NULL);

	// 3. Híbrido A: Titulo e Subtitulo em Flash, resto em RAM (Para menus fixos com valores)
	void mostrarTelaMensagem(const __FlashStringHelper *l1, const __FlashStringHelper *l2, const char *l3, const char *l4);

	// 4. Híbrido B: Só Título em Flash, resto em RAM (O QUE FALTAVA!)
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