#ifndef TESTCONTROLLER_H
#define TESTCONTROLLER_H

#include "JigaHardware.h"

// Resposta da placa ao comando 0x04 0x81 (versão + tensões medidas pela placa)
struct LeituraPlaca
{
	bool valida;	   // CRC ok, comando 0x04 e tamanho mínimo
	uint8_t status;	   // byte 1 (0x81 / 0x15)
	uint8_t fw;	   // byte 2
	float vFonte;	   // bytes 7-8, mV -> V
	float vBat;	   // bytes 9-10, mV -> V
	uint8_t bytes[16]; // pacote completo convertido (inclui CRC)
	uint8_t nBytes;
};

enum EstadoSistema
{
	ST_MENU,
	ST_MANUAL,
	ST_AMS_TEST, // <--- Novo Estado
	ST_INIT_TEST,
	ST_COM_UART,
	ST_POWER_CHECK,
	ST_POWER_TEST,
	ST_LED_PREP,
	ST_LED_TEST,
	ST_APROVADO,
	ST_REPROVADO,
	ST_DIVERGENCIA // Leitura da placa diverge da jiga: operador aceita ou reprova
};

class TestController
{
private:
	JigaHardware &jiga;
	EstadoSistema estadoAtual;

	// Variáveis de fluxo
	unsigned long timerEstado;
	int opcaoMenu;
	int passoLed;
	bool flagEntradaEstado;
	bool habilitarTesteCCPS; // <--- NOVA FLAG CLARA E DESCRITIVA
				 // --- VARIÁVEIS DE SEGURANÇA (NOVO) ---
	float alvoVoutAtual;
	unsigned long timerCurto;

	// --- CONSULTA DA PLACA / DIVERGÊNCIA ---
	bool consultaPendente;	// LED_PREP arma uma consulta por passo
	bool consultaEnviada;
	unsigned long tConsulta;
	uint8_t tentativasConsulta; // reenvios após resposta inválida/ausente
	bool confirmandoDiv;	    // divergiu 1x: reconsulta antes de alarmar
	float placaBatPasso;	    // V.Bat lido da placa no passo atual (-1 = ainda não lido)
	EstadoSistema estadoRetornoDiv; // para onde ENTER volta na tela de divergência
	float alvoDiv;			// tensão mantida enquanto a tela de divergência está aberta
	float divPlacaFonte, divJigaFonte, divPlacaBat, divJigaBat;

	// Configuração de Testes
	static const int TOTAL_PASSOS_LED = 10;
	const float listaTensoes[10] = {
	    V_TEST_UNDER, V_TEST_0_FRESH_BOOT, V_TEST_50_FRESH_BOOT, V_TEST_0_FRESH_BOOT, V_TEST_0_25, V_TEST_25_50, V_TEST_50_75,
	    V_TEST_75_100, V_TEST_OVER, V_TEST_CRITICAL};
	const char *listaMsgs[10] = {
	    "LEDS APAGADOS P1?", "VM PISCA RAPIDO?", "VD PISCA LENTO?", "LEDS APAGADOS P2?", "VM PISCA RAPIDO?", "VM PISCA LENTO?",
	    "VD PISCA LENTO?", "VD PISCA RAPIDO?", "PISCA ALTERNADO?", "PISCA JUNTOS?"};

	// Handlers de Estado
	void handleMenu(Tecla t);
	void handleManual(Tecla t, LeituraSensores &s);
	void amsTest(Tecla t, LeituraSensores &s); // <--- Declaração do teste AMS
	void handleInitTest();
	void handleComUart(LeituraSensores &s);
	void handlePowerCheck(LeituraSensores &s);
	void handlePowerTest(Tecla t, LeituraSensores &s);
	void handleLedPrep(LeituraSensores &s);
	void handleLedTest(Tecla t, LeituraSensores &s);
	void handleAprovado(Tecla t);
	void handleReprovado(Tecla t);

	// --- MÉTODOS DE SEGURANÇA (NOVO) ---
	bool verificarCurtoCircuito(float alvo, float atual);
	void dispararErroCurto();

	void resetarSistema();

	// --- CONSULTA DA PLACA (comando 0x04) ---
	void enviarConsultaPlaca();
	bool lerConsultaPlaca(LeituraPlaca &out); // true se chegou algum pacote
	void logarComparacao(int passo, float alvo, const LeituraPlaca &p, const LeituraSensores &s);
	float valorCanal(CanalRef canal, const LeituraSensores &s);
	bool leituraPlacaDiverge(const LeituraPlaca &p, const LeituraSensores &s);
	void abrirDivergencia(EstadoSistema retorno, float alvo);
	void handleDivergencia(Tecla t, LeituraSensores &s);
	void mostrarLeituraPlacaLcd();

public:
	TestController(JigaHardware &hw);
	void begin();
	void run();
};

#endif