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
	bool estadoFisico;
	bool estadoLogico;
	unsigned long ultimoTempo;
};

class JigaHardware
{
private:
	LiquidCrystal_I2C lcd;
	SoftwareSerial *serialSoft;
	int pwmAtual;
	unsigned long tUltimoAjuste;
	float alvoTravado;
	bool tensaoTravada;
	static const int TAM_BUFFER = 64;
	uint8_t bufferRaw[TAM_BUFFER];
	int tamanhoBufferRaw;
	EstadoBotao btnUp, btnDown, btnRight, btnLeft;
	Tecla checarBotao(int pino, Tecla id, EstadoBotao &estado);
	float lerADC(int pino);

public:
	JigaHardware();
	void begin();

	// Atuadores
	void setCarga(bool estado);
	void setVbat(bool estado);
	void setBateriaCC(bool estado);
	void desligarTudo();
	void controlarAlvo(bool ligado);
	void setPWM(int valor);
	int getPWM();
	bool ajustarTensaoAutomatico(float alvo, float valorAtual);

	// Comunicação
	void enviarBytesComoASCII(uint8_t *dados, size_t tamanho);
	bool lerPacoteBinario();
	uint8_t *getBufferBytes();
	size_t getTamanhoBuffer();
	void limparBufferUART();

	// Interface
	Tecla lerTeclado();
	bool checarResetEmergencia();
	void limparTela();

	// --- FUNÇÕES DE TEXTO ---
	void escreverLinha(int linha, const char *texto); // <--- A FUNÇÃO QUE FALTAVA
	void atualizarParteTela(int linha, int coluna, const char *texto);

	// --- TEMPLATES DE MENSAGEM (Coringa para Flash/RAM) ---
	template <typename T1, typename T2, typename T3, typename T4>
	void mostrarTelaMensagem(T1 l1, T2 l2, T3 l3, T4 l4)
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

	template <typename T1, typename T2, typename T3>
	void mostrarTelaMensagem(T1 l1, T2 l2, T3 l3)
	{
		mostrarTelaMensagem(l1, l2, l3, "");
	}

	template <typename T1, typename T2>
	void mostrarTelaMensagem(T1 l1, T2 l2)
	{
		mostrarTelaMensagem(l1, l2, "", "");
	}

	// Áudio
	void beep();
	void beepSucesso();
	void beepErro();
	void tocarMelodiaVitoria();
	void tocarMelodiaDerrota();

	// Sensores
	LeituraSensores lerTudo();
};

#endif