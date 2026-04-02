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
	jiga.mostrarTelaMensagem(F("INICIANDO SISTEMA..."), F("Jiga CCPS - v3.0"), F("Carregando..."), F(""));
	delay(1000);
	estadoAtual = ST_MENU;
}

void TestController::run()
{
	// 1. Leitura de Entradas
	Tecla t = jiga.lerTeclado();
	LeituraSensores s = jiga.lerTudo();

	// 2. Segurança Global
	if (jiga.checarResetEmergencia())
		resetarSistema();

	// 3. Máquina de Estados Principal
	switch (estadoAtual)
	{
	case ST_MENU:
		handleMenu(t);
		break;
	case ST_MANUAL:
		handleManual(t, s);
		break;
	case ST_AMS_TEST:
		amsTest(t, s);
		break; // <--- Novo Teste Integrado
	case ST_INIT_TEST:
		handleInitTest();
		break;
	case ST_POWER_CHECK:
		handlePowerCheck(s);
		break;

	case ST_LEITURA_UART: // <--- ADICIONE ESTAS 3 LINHAS
		handleLeituraUart();
		break;
	case ST_POWER_TEST:
		handlePowerTest(t, s);
		break;
	case ST_LED_PREP:
		handleLedPrep(s);
		break;
	case ST_LED_TEST:
		handleLedTest(t, s);
		break;
	case ST_APROVADO:
		handleAprovado(t);
		break;
	case ST_REPROVADO:
		handleReprovado(t);
		break;
	}
}

// ==========================================
// IMPLEMENTAÇÃO DOS ESTADOS
// ==========================================

void TestController::handleMenu(Tecla t)
{
	static int menuAnterior = -1;

	// Atualiza a tela apenas se necessário
	if (opcaoMenu != menuAnterior || t != NENHUMA || !flagEntradaEstado)
	{
		jiga.limparTela();
		jiga.escreverLinha(0, "--- MENU INICIAL ---");

		// Renderiza as 3 opções nas linhas 1, 2 e 3
		if (opcaoMenu == 0)
			jiga.escreverLinha(1, "> 1. TESTE CCPS/CIR");
		else
			jiga.escreverLinha(1, "  1. TESTE CCPS/CIR");

		if (opcaoMenu == 1)
			jiga.escreverLinha(2, "> 2. TESTE ECO/DIR");
		else
			jiga.escreverLinha(2, "  2. TESTE ECO/DIR");

		if (opcaoMenu == 2)
			jiga.escreverLinha(3, "> 3. MODO MANUAL");
		else
			jiga.escreverLinha(3, "  3. MODO MANUAL");

		menuAnterior = opcaoMenu;
		flagEntradaEstado = true;
	}

	// Navegação (0 -> 1 -> 2 -> 0)
	if (t == TECLA_BAIXO)
	{
		opcaoMenu++;
		if (opcaoMenu > 2)
			opcaoMenu = 0;
		jiga.beep();
	}
	if (t == TECLA_CIMA)
	{
		opcaoMenu--;
		if (opcaoMenu < 0)
			opcaoMenu = 2;
		jiga.beep();
	}
	if (t == TECLA_ENTER)
	{
		jiga.beepSucesso();
		flagEntradaEstado = false;

		if (opcaoMenu == 0)
		{
			habilitarTesteCCPS = true; // A opção 0 testa placas COM CCPS
			estadoAtual = ST_AMS_TEST;
		}
		else if (opcaoMenu == 1)
		{
			habilitarTesteCCPS = false; // A opção 1 testa placas SEM CCPS
			estadoAtual = ST_INIT_TEST; // Vai pro mesmo teste, mas pulando a etapa!
		}
		else if (opcaoMenu == 2)
		{
			habilitarTesteCCPS = false; // A opção 1 testa placas SEM CCPS
			estadoAtual = ST_MANUAL;    // Vai pro mesmo teste, mas pulando a etapa!
		}
	}
}

// void TestController::amsTest(Tecla t, LeituraSensores &s)
// {
// 	// Enum interno atualizado com erro de Curto
// 	enum SubEstadoAMS
// 	{
// 		AMS_INIT,
// 		AMS_RAMP_20V,
// 		AMS_CHECK_20V,
// 		AMS_RAMP_25V,
// 		AMS_HOLD_25V,
// 		AMS_PASS,
// 		AMS_FAIL_PIRATE, // Erro de tensão 3v3 (Pirata)
// 		AMS_FAIL_SHORT	 // Erro de Rampa (Curto/Sobrecarga)
// 	};

// 	static SubEstadoAMS subState = AMS_INIT;
// 	static unsigned long tTimer = 0; // Usado para Timeout da Rampa E para Contagem de 5s
// 	static unsigned long tUpdateDisplay = 0;
// 	static bool erroMostrado = false;

// 	// --- SETUP INICIAL ---
// 	if (!flagEntradaEstado)
// 	{
// 		jiga.desligarTudo();
// 		jiga.limparTela();
// 		jiga.mostrarTelaMensagem(F("TESTE DO AMS"), F("PREPARE-SE..."), F("Nao mexa na placa"), F(""));
// 		delay(1000);

// 		subState = AMS_INIT;
// 		erroMostrado = false;
// 		flagEntradaEstado = true;
// 	}

// 	// --- BOTÃO DE PÂNICO ---
// 	if (t == TECLA_ESC)
// 	{
// 		jiga.desligarTudo();
// 		jiga.setPWM(255);
// 		jiga.mostrarTelaMensagem(F("TESTE ABORTADO"), F("Seguranca ativada"), F(""), F(""));
// 		jiga.beepErro();
// 		delay(1000);
// 		flagEntradaEstado = false;
// 		estadoAtual = ST_MENU;
// 		return;
// 	}

// 	switch (subState)
// 	{
// 	case AMS_INIT:
// 		jiga.setVbat(true);	  // Habilita entrada de tensão
// 		jiga.controlarAlvo(true); // Habilita a placa (opcional, dependendo do seu circuito)

// 		tTimer = millis(); // <--- INICIA O CRONÔMETRO PARA A RAMPA DE 20V
// 		subState = AMS_RAMP_20V;
// 		break;

// 	case AMS_RAMP_20V:
// 		// 1. Atualiza Display
// 		if (millis() - tUpdateDisplay > 200)
// 		{
// 			char msg[21];
// 			sprintf(msg, "Alvo: 20V  At: %d.%d", (int)s.vLoad, (int)(s.vLoad * 10) % 10);
// 			jiga.mostrarTelaMensagem(F("ETAPA 1: SUBINDO"), F("Buscando 20V..."), msg, F(""));
// 			tUpdateDisplay = millis();
// 		}

// 		// 2. Proteção de Timeout (Curto-circuito)
// 		if (millis() - tTimer > 2000) // Se passar de 2 segundos tentando subir
// 		{
// 			subState = AMS_FAIL_SHORT; // ABORTA
// 			return;
// 		}

// 		// 3. Tenta Ajustar
// 		if (jiga.ajustarTensaoAutomatico(V_AMS_CHECK, s.vLoad))
// 		{
// 			// Chegou em 20V!
// 			tTimer = millis(); // Reseta timer para usar no próximo estado (Estabilização)
// 			subState = AMS_CHECK_20V;
// 		}
// 		break;

// 	case AMS_CHECK_20V:
// 		jiga.ajustarTensaoAutomatico(V_AMS_CHECK, s.vLoad);

// 		// Espera 1s estabilizar
// 		if (millis() - tTimer > 1000)
// 		{
// 			// Verifica o 3.3V
// 			if (s.v3v3 >= V_3V3_MIN && s.v3v3 <= V_3V3_MAX)
// 			{
// 				jiga.beepSucesso();
// 				tTimer = millis(); // <--- INICIA O CRONÔMETRO PARA A RAMPA DE 25V
// 				subState = AMS_RAMP_25V;
// 			}
// 			else
// 			{
// 				subState = AMS_FAIL_PIRATE;
// 			}
// 		}
// 		break;

// 	case AMS_RAMP_25V:
// 		// 1. Atualiza Display
// 		if (millis() - tUpdateDisplay > 200)
// 		{
// 			char msg[21];
// 			sprintf(msg, "Alvo: 25V  At: %d.%d", (int)s.vLoad, (int)(s.vLoad * 10) % 10);
// 			jiga.mostrarTelaMensagem(F("ETAPA 2: STRESS"), F("Subindo p/ 25V..."), msg, F("ATENCAO!"));
// 			tUpdateDisplay = millis();
// 		}

// 		// 2. Proteção de Timeout (Curto-circuito nos 25V) - AQUI É O MAIS IMPORTANTE
// 		if (millis() - tTimer > 2000)
// 		{
// 			subState = AMS_FAIL_SHORT; // ABORTA SE NÃO SUBIR EM 2s
// 			return;
// 		}

// 		// 3. Tenta Ajustar
// 		if (jiga.ajustarTensaoAutomatico(V_AMS_STRESS, s.vLoad))
// 		{
// 			// Chegou em 25V!
// 			tTimer = millis(); // Inicia timer dos 5 segundos de hold
// 			subState = AMS_HOLD_25V;
// 		}
// 		break;

// 	case AMS_HOLD_25V:
// 		jiga.ajustarTensaoAutomatico(V_AMS_STRESS, s.vLoad);

// 		// Monitoramento contínuo da saída 3V3
// 		if (s.v3v3 < V_3V3_MIN || s.v3v3 > V_3V3_MAX)
// 		{
// 			subState = AMS_FAIL_PIRATE;
// 			return;
// 		}

// 		if (millis() - tUpdateDisplay > 200)
// 		{
// 			int restantes = 5 - (millis() - tTimer) / 1000;
// 			char msg[21];
// 			sprintf(msg, "Segure: %ds", restantes);
// 			char msgV33[21];
// 			dtostrf(s.v3v3, 4, 2, msgV33);
// 			char msgLinha4[21];
// 			sprintf(msgLinha4, "3V3: %sV (OK)", msgV33);
// 			jiga.mostrarTelaMensagem(F("TESTE DESTRUTIVO"), F("Aplicando 25V..."), msg, msgLinha4);
// 			tUpdateDisplay = millis();
// 		}

// 		if (millis() - tTimer > 5000)
// 		{
// 			subState = AMS_PASS;
// 		}
// 		break;

// 	case AMS_PASS:
// 		jiga.desligarTudo();
// 		jiga.setPWM(255);
// 		static bool flagMusic = true;

// 		if (millis() - tUpdateDisplay > 1000)
// 		{

// 			jiga.mostrarTelaMensagem(F("AMS ORIGINAL"), F("APROVADO (25V OK)"), F("Regulador Robusto"), F("ENTER p/ Continuar"));

// 			if (flagMusic)
// 			{
// 				jiga.tocarMelodiaVitoria();
// 				flagMusic = false;
// 			}

// 			tUpdateDisplay = millis();
// 		}

// 		if (t == TECLA_ENTER)
// 		{
// 			flagMusic = true;
// 			flagEntradaEstado = false;
// 			estadoAtual = ST_INIT_TEST;
// 		}
// 		break;

// 	// --- CASO 1: TENSÃO SUBIU, MAS REGULADOR FALHOU (PIRATA) ---
// 	case AMS_FAIL_PIRATE:
// 		if (!erroMostrado)
// 		{
// 			jiga.desligarTudo();
// 			jiga.setPWM(255);
// 			jiga.mostrarTelaMensagem(F("FALHA: AMS PIRATA"), F("Trocar Imediato!"), F("Nao aguentou carga"), F("PRESSIONE BOTAO"));
// 			jiga.beepErro();
// 			erroMostrado = true;
// 		}
// 		if (t != NENHUMA)
// 		{
// 			flagEntradaEstado = false;
// 			estadoAtual = ST_REPROVADO;
// 		}
// 		break;

// 	// --- CASO 2: TENSÃO NEM SUBIU (CURTO-CIRCUITO / SOBRECARGA) ---
// 	case AMS_FAIL_SHORT:
// 		if (!erroMostrado)
// 		{
// 			// DESLIGA TUDO IMEDIATAMENTE
// 			jiga.desligarTudo();
// 			jiga.setPWM(255);

// 			// Mensagem específica de Curto
// 			jiga.mostrarTelaMensagem(F("FALHA GRAVE: CURTO"), F("Tensao nao subiu!"), F("Verifique a placa"), F("PRESSIONE BOTAO"));

// 			// Beep longo e contínuo para alertar
// 			jiga.beepErro();
// 			delay(200);
// 			jiga.beepErro();

// 			erroMostrado = true;
// 		}
// 		if (t != NENHUMA)
// 		{
// 			flagEntradaEstado = false;
// 			estadoAtual = ST_REPROVADO;
// 		}
// 		break;
// 	}
// }

void TestController::amsTest(Tecla t, LeituraSensores &s)
{
	enum SubEstadoAMS
	{
		AMS_INIT,
		AMS_RAMP_20V,
		AMS_CHECK_20V,
		AMS_RAMP_25V,
		AMS_HOLD_25V,
		AMS_PASS,
		AMS_FAIL_PIRATE,
		AMS_FAIL_POWER
	};

	static SubEstadoAMS subState = AMS_INIT;
	static unsigned long tTimer = 0;
	static unsigned long tUpdateDisplay = 0;

	static bool erroMostrado = false;
	static bool musicaTocada = false;
	static bool isCir = false;

	if (!flagEntradaEstado)
	{
		Serial.println(F("\n=================================="));
		Serial.println(F("  >>> INICIANDO NOVO TESTE AMS <<<  "));
		Serial.println(F("=================================="));

		jiga.desligarTudo();
		jiga.limparTela();
		jiga.mostrarTelaMensagem(F("TESTE DO AMS"), F("PREPARE-SE..."), F("Nao mexa na placa"), F(""));
		delay(1000);

		subState = AMS_INIT;
		erroMostrado = false;
		musicaTocada = false;
		isCir = false;
		flagEntradaEstado = true;
	}

	if (t == TECLA_ESC)
	{
		Serial.println(F("[SISTEMA] Teste Abortado pelo botao ESC!"));
		jiga.desligarTudo();
		jiga.setPWM(255);
		jiga.mostrarTelaMensagem(F("TESTE ABORTADO"), F("Seguranca ativada"), F(""), F(""));
		jiga.beepErro();
		delay(1000);
		flagEntradaEstado = false;
		estadoAtual = ST_MENU;
		return;
	}

	switch (subState)
	{
	case AMS_INIT:
		Serial.println(F("[ESTADO] AMS_INIT"));
		Serial.println(F(" -> Mantendo Vbat desligado para degrau"));
		Serial.println(F(" -> Acordando a placa (controlarAlvo)"));

		jiga.setVbat(false);
		jiga.controlarAlvo(false);
		subState = AMS_RAMP_20V;
		break;

	case AMS_RAMP_20V:
		if (millis() - tUpdateDisplay > 500)
		{
			char msg[21];
			sprintf(msg, "Alvo: 20V  At: %d.%d", (int)s.vLoad, (int)(s.vLoad * 10) % 10);
			jiga.mostrarTelaMensagem(F("ETAPA 1: SUBINDO"), F("Buscando 20V..."), msg, F("Placa Desligada"));

			Serial.print(F("[RAMPA 20V] PWM: "));
			Serial.print(jiga.getPWM());
			Serial.print(F(" | vLoad: "));
			Serial.print(s.vLoad);
			Serial.print(F("V | v3v3: "));
			Serial.println(s.v3v3);

			tUpdateDisplay = millis();
		}

		if (verificarCurtoCircuito(V_AMS_CHECK, s.vLoad))
		{
			Serial.println(F("[ERRO] Watchdog de curto desarmou na Rampa 20V!"));
			dispararErroCurto();
			return;
		}

		jiga.ajustarTensaoAutomatico(V_AMS_CHECK, s.vLoad);

		if (s.vLoad >= (V_AMS_CHECK - 0.4) && s.vLoad <= (V_AMS_CHECK + 0.4))
		{
			Serial.println(F("\n[RAMPA 20V] *** ALVO 20V ATINGIDO! ***"));
			Serial.println(F(" -> Ligando relé do Vbat (Degrau de Tensão)"));
			jiga.setVbat(true);
			tTimer = millis();
			subState = AMS_CHECK_20V;
		}
		break;

	case AMS_CHECK_20V:
		if (verificarCurtoCircuito(V_AMS_CHECK, s.vLoad))
		{
			dispararErroCurto();
			return;
		}
		jiga.ajustarTensaoAutomatico(V_AMS_CHECK, s.vLoad);

		if (millis() - tUpdateDisplay > 500)
		{
			char msg[21], msgV33[21], linha4[21];
			sprintf(msg, "V-Jiga: %d.%d V", (int)s.vLoad, (int)(s.vLoad * 10) % 10);
			dtostrf(s.v3v3, 4, 2, msgV33);
			sprintf(linha4, "Lido 3V3: %sV", msgV33);

			jiga.mostrarTelaMensagem(F("AVALIANDO PLACA..."), msg, F("Aguarde 4 seg..."), linha4);

			Serial.print(F("[CHECK 20V] Aguardando... Tempo: "));
			Serial.print(millis() - tTimer);
			Serial.print(F("ms | vLoad: "));
			Serial.print(s.vLoad);
			Serial.print(F(" | v3v3 (Real): "));
			Serial.println(s.v3v3);

			tUpdateDisplay = millis();
		}

		if (millis() - tTimer > 4000)
		{
			// Aqui fazemos as validações e jogamos direto os resultados finais
			if (s.v3v3 <= 1.0)
			{
				Serial.println(F(" -> DECISAO: Falha de Energia (v3v3 <= 1.0)"));
				subState = AMS_FAIL_POWER;
			}
			else if (s.v3v3 >= V_3V3_MIN && s.v3v3 <= V_3V3_MAX)
			{
				Serial.println(F(" -> DECISAO: Aprovado! Placa Padrao 3.3V"));
				isCir = false;
				jiga.beepSucesso();
				tTimer = millis();
				subState = AMS_RAMP_25V;
			}
			else if (s.v3v3 >= 4.7 && s.v3v3 <= 5.3)
			{
				Serial.println(F(" -> DECISAO: Aprovado! Placa CIR 5.0V"));
				isCir = true;
				jiga.beepSucesso();
				tTimer = millis();
				subState = AMS_RAMP_25V;
			}
			else
			{
				Serial.print(F(" -> DECISAO: Falha! AMS Pirata. Lido: "));
				Serial.println(s.v3v3);
				subState = AMS_FAIL_PIRATE;
			}
		}
		break;

	case AMS_RAMP_25V:
		if (millis() - tUpdateDisplay > 500)
		{
			char msg[21];
			sprintf(msg, "Alvo: 25V  At: %d.%d", (int)s.vLoad, (int)(s.vLoad * 10) % 10);
			jiga.mostrarTelaMensagem(F("ETAPA 2: STRESS"), F("Subindo p/ 25V..."), msg, F("ATENCAO!"));

			Serial.print(F("[RAMPA 25V] vLoad: "));
			Serial.print(s.vLoad);
			Serial.print(F("V | v3v3: "));
			Serial.println(s.v3v3);
			tUpdateDisplay = millis();
		}

		if (verificarCurtoCircuito(V_AMS_STRESS, s.vLoad))
		{
			dispararErroCurto();
			return;
		}
		jiga.ajustarTensaoAutomatico(V_AMS_STRESS, s.vLoad);

		if (s.vLoad >= (V_AMS_STRESS - 0.5) && s.vLoad <= (V_AMS_STRESS + 0.5))
		{
			Serial.println(F("[RAMPA 25V] Alvo atingido! Iniciando stress..."));
			tTimer = millis();
			subState = AMS_HOLD_25V;
		}
		break;

	case AMS_HOLD_25V:
	{
		if (verificarCurtoCircuito(V_AMS_STRESS, s.vLoad))
		{
			dispararErroCurto();
			return;
		}
		jiga.ajustarTensaoAutomatico(V_AMS_STRESS, s.vLoad);

		float minPermitido = isCir ? 4.7 : V_3V3_MIN;
		float maxPermitido = isCir ? 5.3 : V_3V3_MAX;

		if (s.v3v3 < minPermitido || s.v3v3 > maxPermitido)
		{
			Serial.println(F("[HOLD 25V] ERRO! Regulador estourou no stress."));
			subState = AMS_FAIL_PIRATE;
			return;
		}

		if (millis() - tUpdateDisplay > 500)
		{
			int restantes = 5 - (millis() - tTimer) / 1000;
			char msg[21], msgV33[21], msgLinha4[21];

			sprintf(msg, "Espere: %ds", restantes);
			dtostrf(s.v3v3, 4, 2, msgV33);

			if (isCir)
				sprintf(msgLinha4, "5V: %sV (CIR)", msgV33);
			else
				sprintf(msgLinha4, "3V3: %sV (OK)", msgV33);

			jiga.mostrarTelaMensagem(F("TESTE DESTRUTIVO"), F("Aplicando 25V..."), msg, msgLinha4);

			Serial.print(F("[HOLD 25V] Restam: "));
			Serial.print(restantes);
			Serial.print(F("s | v3v3 atual: "));
			Serial.println(s.v3v3);
			tUpdateDisplay = millis();
		}

		if (millis() - tTimer > 5000)
		{
			Serial.println(F("[HOLD 25V] Sucesso! Sobreviveu aos 25V."));
			subState = AMS_PASS;
		}
		break;
	}

	case AMS_PASS:
		jiga.desligarTudo();
		jiga.setPWM(255);

		if (!musicaTocada)
		{
			Serial.println(F("[FIM] Teste Aprovado!"));
			if (isCir)
			{
				jiga.mostrarTelaMensagem(F("PLACA CIR (5V)"), F("APROVADO (25V OK)"), F("Regulador Robusto"), F("ENTER p/ Continuar"));
			}
			else
			{
				jiga.mostrarTelaMensagem(F("AMS ORIGINAL"), F("APROVADO (25V OK)"), F("Regulador Robusto"), F("ENTER p/ Continuar"));
			}

			jiga.tocarMelodiaVitoria();
			musicaTocada = true;
		}

		if (t == TECLA_ENTER)
		{
			flagEntradaEstado = false;
			estadoAtual = ST_INIT_TEST;
		}
		break;

	case AMS_FAIL_POWER:
		if (!erroMostrado)
		{
			jiga.desligarTudo();
			jiga.setPWM(255);
			jiga.mostrarTelaMensagem(F("ERRO: SEM ALIMENTACAO"), F("Tensao < 1.0V"), F("Verifique conexoes!"), F("PRESSIONE BOTAO"));
			jiga.beepErro();
			erroMostrado = true;
		}
		if (t != NENHUMA)
		{
			flagEntradaEstado = false;
			estadoAtual = ST_REPROVADO;
		}
		break;

	case AMS_FAIL_PIRATE:
		if (!erroMostrado)
		{
			jiga.desligarTudo();
			jiga.setPWM(255);
			jiga.mostrarTelaMensagem(F("FALHA: AMS PIRATA"), F("Trocar Imediato!"), F("Nao aguentou carga"), F("PRESSIONE BOTAO"));
			jiga.beepErro();
			erroMostrado = true;
		}
		if (t != NENHUMA)
		{
			flagEntradaEstado = false;
			estadoAtual = ST_REPROVADO;
		}
		break;
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
		jiga.setVbat(cargaLigada);
		jiga.beepSucesso();
	}
	if (t == TECLA_ESC)
	{
		jiga.desligarTudo();
		jiga.setPWM(255);
		estadoAtual = ST_MENU;
		flagEntradaEstado = false;
	}

	// Atualização da tela a cada 200ms
	if (millis() - tUpdate > 200)
	{
		char l1[21], l2[21], l3[21];
		sprintf(l1, "MANUAL PWM:%d", jiga.getPWM());

		// Variáveis para converter os floats em texto
		char vLoadStr[6], vCapStr[6], v3v3Str[6];

		dtostrf(s.vLoad, 4, 1, vLoadStr); // Formata Vout com 1 casa (ex: 12.0)
		dtostrf(s.vCap, 4, 1, vCapStr);	  // Formata VCap com 1 casa (ex: 11.5)
		dtostrf(s.v3v3, 4, 2, v3v3Str);	  // Formata 3V3 com 2 casas (ex: 3.33)

		// Montagem da Linha 2 (Tensão de Saída e Status)
		sprintf(l2, "V_OUT: %sV %s", vLoadStr, cargaLigada ? "ON " : "OFF");

		// Montagem da Linha 3 (Capacitor e 3V3 lado a lado)
		// Fica no formato: "VCap:12.5 3V3:3.30V" (Ocupa ~19 de 20 caracteres)
		sprintf(l3, "VCap:%s 3V3:%sV", vCapStr, v3v3Str);

		jiga.mostrarTelaMensagem(l1, l2, l3, "ESC p/ Sair");
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
	estadoAtual = ST_LEITURA_UART;
}

// void TestController::handleComUart()
// {
// 	static bool enviado = false;

// 	if (!flagEntradaEstado)
// 	{
// 		jiga.limparTela();
// 		jiga.mostrarTelaMensagem(F("1. TESTE SERIAL"), F("Conectando..."), F("Binario + CRC"), F(""));
// 		jiga.controlarAlvo(true);
// 		jiga.desligarTudo();
// 		// jiga.setVbat(false);
// 		// jiga.setCarga(true);
// 		jiga.setVbat(true);
// 		jiga.setCarga(false);
// 		jiga.limparBufferUART();
// 		timerEstado = millis();
// 		enviado = false;
// 		flagEntradaEstado = true;
// 	}

// 	if (!enviado && millis() - timerEstado > 1000)
// 	{
// 		uint8_t pacote[10];
// 		pacote[0] = 0x04;
// 		pacote[1] = 0x81;
// 		size_t len = Protocolo::anexarCRC(pacote, 2);
// 		jiga.enviarBytesComoASCII(pacote, len);
// 		enviado = true;
// 		timerEstado = millis();
// 	}

// 	if (enviado)
// 	{
// 		if (jiga.lerPacoteBinario())
// 		{
// 			uint8_t *bufRaw = jiga.getBufferBytes();
// 			size_t tamRaw = jiga.getTamanhoBuffer();
// 			RespostaInterpretada resp = Protocolo::interpretarPacote(bufRaw, tamRaw);

// 			if (resp.crcValido)
// 			{
// 				uint8_t dados[32];
// 				int idx = 0;

// 				// Conversão Hex -> Byte usando Helper do Protocolo
// 				for (size_t i = 0; i < tamRaw; i += 2)
// 				{
// 					if (i + 1 >= tamRaw || idx >= 32)
// 						break;
// 					uint8_t h = Protocolo::charParaByte(bufRaw[i]);
// 					uint8_t l = Protocolo::charParaByte(bufRaw[i + 1]);
// 					dados[idx++] = (h << 4) | l;
// 				}

// 				uint8_t fwVersion = dados[2];
// 				uint16_t vFonte = (dados[7] << 8) | dados[8];
// 				uint16_t vBat = (dados[9] << 8) | dados[10];

// 				if (fwVersion == 0x04 && (resp.status == 0x81 || resp.status == 0x15))
// 				{
// 					char msgFonte[21], msgBat[21];
// 					sprintf(msgFonte, "V.Fonte: %d", vFonte);
// 					sprintf(msgBat, "V.Bat:   %d", vBat);
// 					jiga.mostrarTelaMensagem(F("PLACA COMPATIVEL"), F("FW: v04 Detectado"), msgFonte, msgBat);

// 					jiga.beepSucesso();
// 					delay(3000);
// 					flagEntradaEstado = false;
// 					estadoAtual = ST_POWER_CHECK;
// 				}
// 				else
// 				{
// 					char msgErro[21];
// 					sprintf(msgErro, "Lido: v%02X", fwVersion);
// 					jiga.mostrarTelaMensagem(F("ERRO VERSAO FW"), F("Esperado: v04"), msgErro, "Falha Serial");
// 					jiga.beepErro();
// 					delay(3000);
// 					estadoAtual = ST_REPROVADO;
// 				}
// 			}
// 			else
// 			{
// 				jiga.mostrarTelaMensagem(F("ERRO CRC"), F("Dados Corrompidos"), F(""), F(""));
// 				jiga.beepErro();
// 				estadoAtual = ST_REPROVADO;
// 			}
// 		}
// 		if (millis() - timerEstado > 5000)
// 		{
// 			jiga.mostrarTelaMensagem(F("TIMEOUT"), F("Sem resposta"), F(""), F(""));
// 			jiga.beepErro();
// 			flagEntradaEstado = false;
// 			estadoAtual = ST_REPROVADO;
// 		}
// 	}
// }

// Função igual a de cima, mas com propostas de melhoria:
// void TestController::handleLeituraUart()
// {
// 	static bool leituraConcluida = false;
// 	static unsigned long timerExibicao = 0; // A nossa variável da "Fotografia"

// 	// 1. INICIALIZAÇÃO DA TELA (Roda só 1 vez)
// 	if (!flagEntradaEstado)
// 	{
// 		jiga.controlarAlvo(true);
// 		jiga.limparTela();
// 		jiga.mostrarTelaMensagem(F("LENDO PLACA..."), F("Aguarde..."), F(""), F(""));

// 		jiga.limparBufferUART();

// 		uint8_t pacote[2] = {0x04, 0x81};
// 		size_t len = Protocolo::anexarCRC(pacote, 2);
// 		jiga.enviarBytesComoASCII(pacote, len);

// 		leituraConcluida = false;
// 		flagEntradaEstado = true;
// 	}

// 	// 2. FASE DE LEITURA E GATILHO
// 	if (!leituraConcluida)
// 	{
// 		if (jiga.lerPacoteBinario())
// 		{
// 			// [CÓDIGO DE PROTOCOLO E CRC OCULTADOS AQUI]

// 			uint8_t fwVersion = dados[2];
// 			uint16_t vFonte = (dados[7] << 8) | dados[8];
// 			uint16_t vBat = (dados[9] << 8) | dados[10];

// 			if (fwVersion >= 0x04) // Sua ideia de compatibilidade futura!
// 			{
// 				char msgFonte[21], msgBat[21];
// 				sprintf(msgFonte, "V.Fonte: %d", vFonte);
// 				sprintf(msgBat, "V.Bat:   %d", vBat);
// 				jiga.mostrarTelaMensagem(F("DADOS RECEBIDOS"), F("FW: OK"), msgFonte, msgBat);

// 				jiga.beepSucesso();

// 				leituraConcluida = true; // Libera a Fase 3

// 				// --- RESPOSTA DA PERGUNTA 1 (O Gatilho) ---
// 				// Tiro a "fotografia" da hora exata que a leitura deu certo
// 				timerExibicao = millis();
// 			}
// 			else
// 			{
// 				estadoAtual = ST_REPROVADO; // FW incompatível
// 			}
// 		}
// 	}

// 	// 3. FASE DE EXIBIÇÃO (Contando o tempo sem travar o Arduino)
// 	if (leituraConcluida)
// 	{
// 		// --- RESPOSTA DA PERGUNTA 2 (A Checagem Universal) ---
// 		if (millis() - timerExibicao > 5000)
// 		{
// 			// --- RESPOSTA DA PERGUNTA 3 (A Fuga) ---
// 			estadoAtual = ST_POWER_TEST;

// 			flagEntradaEstado = false; // Prepara a casa para o próximo estado
// 		}
// 	}
// }

void TestController::handleLeituraUart()
{
    static bool leituraConcluida = false;
    static bool comandoEnviado = false;
    static unsigned long timerExibicao = 0; // Cronômetro da mensagem de sucesso

    // ==========================================
    // 1. INICIALIZAÇÃO
    // ==========================================
    if (!flagEntradaEstado)
    {
        jiga.limparTela();
        jiga.mostrarTelaMensagem(F("3. LENDO PLACA..."), F("Aguarde..."), F(""), F(""));
        jiga.limparBufferUART();

	jiga.setVbat(true);
	jiga.controlarAlvo(true);
        
        comandoEnviado = false;
        leituraConcluida = false;
        
        // Cronômetro Mestre: Guarda a hora que a placa foi ligada
        timerEstado = millis(); 
        
        flagEntradaEstado = true;
    }

    // ==========================================
    // 2. FASE DE LEITURA (Enquanto não deu sucesso nem timeout)
    // ==========================================
    if (!leituraConcluida)
    {
        // A) Espera 1 segundo de "Boot" antes de enviar a pergunta
        if (!comandoEnviado && (millis() - timerEstado > 2000))
        {
            uint8_t pacote[2] = {0x04, 0x81};
            size_t len = Protocolo::anexarCRC(pacote, 2);
            jiga.enviarBytesComoASCII(pacote, len);
            comandoEnviado = true;
        }

        // B) Se já perguntou, fica ouvindo a resposta
        if (comandoEnviado)
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
                    
                    // Decodifica a sopa de letrinhas (Hex) para Bytes
                    for (size_t i = 0; i < tamRaw; i += 2)
                    {
                        if (i + 1 >= tamRaw || idx >= 32) break;
                        uint8_t h = Protocolo::charParaByte(bufRaw[i]);
                        uint8_t l = Protocolo::charParaByte(bufRaw[i + 1]);
                        dados[idx++] = (h << 4) | l;
                    }

                    // Extrai as informações brutas
                    uint8_t fwVersion = dados[2];
                    uint16_t vFonte = (dados[7] << 8) | dados[8];
                    uint16_t vBat = (dados[9] << 8) | dados[10];

                    if (fwVersion >= 0x04) 
                    {
                        char msgFonte[21], msgBat[21];
                        sprintf(msgFonte, "V.Fonte: %d", vFonte);
                        sprintf(msgBat, "V.Bat:   %d", vBat);
                        
                        jiga.mostrarTelaMensagem(F("DADOS RECEBIDOS"), F("FW: OK"), msgFonte, msgBat);
                        jiga.beepSucesso();

                        leituraConcluida = true; // Libera a Fase 3
                        timerExibicao = millis(); // Tira a "foto" para os 5 segundos!
                    }
                    else
                    {
                        char msgErro[21];
                        sprintf(msgErro, "Lido: v%02X", fwVersion);
                        jiga.mostrarTelaMensagem(F("ERRO VERSAO FW"), F("Incompativel"), msgErro, F("Falha Serial"));
                        jiga.beepErro();
                        delay(3000); // Como é Rota de Fuga/Falha, um delay aqui é aceitável
                        estadoAtual = ST_REPROVADO;
                    }
                }
            }
        }

        // C) TIMEOUT (A Barreira contra Congelamento)
        // Se a placa demorou mais de 4 segundos e nada aconteceu:
        if (millis() - timerEstado > 4000)
        {
            jiga.mostrarTelaMensagem(F("ERRO: TIMEOUT"), F("Sem resposta Serial"), F(""), F(""));
            jiga.beepErro();
            delay(2000);
            flagEntradaEstado = false;
            estadoAtual = ST_REPROVADO;
        }
    }

    // ==========================================
    // 3. FASE DE EXIBIÇÃO (5 Segundos rodando livremente)
    // ==========================================
    if (leituraConcluida)
    {
        if (millis() - timerExibicao > 5000)
        {
            flagEntradaEstado = false;
            estadoAtual = ST_POWER_TEST; // Vai testar o capacitor!
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

	char lido[10], msg[21];
	dtostrf(s.vLoad, 4, 1, lido);
	sprintf(msg, "LIDO: %s V", lido);

	jiga.mostrarTelaMensagem(F("2. POWER CHECK"), F("Alvo: 12.0V"), msg, "Ajustando...");
	// INJETE ESTA LINHA AQUI:
	if (verificarCurtoCircuito(12.0, s.vLoad))
	{
		dispararErroCurto();
		return;
	}
	if (jiga.ajustarTensaoAutomatico(12.0, s.vLoad))
	{
		jiga.beep();
		jiga.setCarga(true);
		flagEntradaEstado = false;
		estadoAtual = ST_LEITURA_UART;
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
	static unsigned long tUpdateDisplay = 0;

	if (!flagEntradaEstado)
	{
		timerEstado = millis();
		tUpdateDisplay = 0;
		jiga.limparTela();
		jiga.mostrarTelaMensagem(F("2. ALIMENTACAO"), F("LED ENTRADA ACESO?"), F("VCap:      V"), F("ESC:REPRO ENTER:APR"));
		jiga.desligarTudo();
		// delay(50);
		// // tentativa de descarregar o capacitor para teste
		// jiga.setBateriaCC(true);
		// delay(200);
		// jiga.setBateriaCC(false);
		// delay(200);
		jiga.setCarga(true);

		flagEntradaEstado = true;
	}

	if (millis() - tUpdateDisplay > 200)
	{
		char vCapStr[10];
		dtostrf(s.vCap, 4, 1, vCapStr);
		jiga.atualizarParteTela(2, 6, vCapStr);
		tUpdateDisplay = millis();
	}

	// if (s.vCap < 5.0 && (millis() - timerEstado > 2000))
	// {
	// 	jiga.setCarga(false);
	// 	jiga.mostrarTelaMensagem(F("FALHA: CAPACITOR"), F("Tensao baixa"), F(""), F(""));
	// 	jiga.beepErro();
	// 	estadoAtual = ST_REPROVADO;
	// }

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

	char alvoStr[10], msg[21];
	dtostrf(listaTensoes[passoLed], 4, 1, alvoStr);
	sprintf(msg, "Alvo: %sV", alvoStr);

	jiga.mostrarTelaMensagem(F("3. AJUSTE LEDS"), msg, "Aguarde...", "");

	if (verificarCurtoCircuito(listaTensoes[passoLed], s.vLoad))
	{
		dispararErroCurto();
		return;
	}
	// Quando a Jiga finalmente estabilizar na tensão alvo:
	if (jiga.ajustarTensaoAutomatico(listaTensoes[passoLed], s.vLoad))
	{
		// --- NOVA VALIDAÇÃO DA SAÍDA DA PLACA --- // tirei por ter um bug ao testar o cir
		// Índices do passoLed:
		// 0 = "LEDS APAGADOS?"
		// 1 = "VM PISCA (0-25)?"
		// 2 = "VM ACESO (25-50)?"  <-- A partir daqui faremos a checagem
		// if ((passoLed > 3 && passoLed <= 5) && habilitarTesteCCPS)
		// {
		// 	Serial.println("Estou no if");
		// 	Serial.println(s.vCCPS);
		// 	// Verifica se a saída do dispositivo (vCCPS) está fora da faixa 11V - 14V
		// 	if (s.vCCPS < 11.0 || s.vCCPS > 16.0)
		// 	{
		// 		// Formata o valor lido para mostrar na tela o motivo do erro
		// 		char lido[10];
		// 		dtostrf(s.vCCPS, 4, 1, lido);
		// 		char msgErro[21];
		// 		sprintf(msgErro, "Lido: %s V", lido);

		// 		// Reprova imediatamente
		// 		jiga.desligarTudo();
		// 		jiga.mostrarTelaMensagem(F("FALHA: SAIDA PLACA"), F("Esp: 11.0V a 14.0V"), msgErro, F("PRESSIONE BOTAO"));
		// 		jiga.beepErro();

		// 		Serial.println("O valor de tensao lido foi:");
		// 		Serial.println(s.vCCPS);

		// 		// --- A MÁGICA DOS 3 SEGUNDOS ESTÁ AQUI ---
		//                 delay(3000);

		// 		flagEntradaEstado = false;

		// 		estadoAtual = ST_REPROVADO;
		// 		return; // Aborta a função para não ir para o ST_LED_TEST
		// 	}
		// }
		// ----------------------------------------

		flagEntradaEstado = false;
		estadoAtual = ST_LED_TEST;
	}
	delay(15);
}


void TestController::handleLedTest(Tecla t, LeituraSensores &s)
{
	static unsigned long tUpdateDisplay = 0;
	static int passoAnterior = -1;

	// --- VARIÁVEIS PARA A VALIDAÇÃO CRUZADA ---
	static unsigned long timerUart = 0;
	static bool aguardandoResposta = false;
	
	static float ultimaLeituraVin = 0.0; 
	static float ultimaLeituraVbat = 0.0;

	// ==========================================
	// 1. CONTROLE DE POTÊNCIA E SEGURANÇA
	// ==========================================
	if (verificarCurtoCircuito(listaTensoes[passoLed], s.vLoad))
	{
		dispararErroCurto();
		return;
	}
	jiga.ajustarTensaoAutomatico(listaTensoes[passoLed], s.vLoad);

	// ==========================================
	// 2. VALIDAÇÃO CRUZADA (A partir do 2º passo: passoLed >= 1)
	// ==========================================
	if (passoLed >= 1)
	{
		// A) Pede a foto a cada 1 segundo
		if (!aguardandoResposta && (millis() - timerUart > 1000))
		{
			jiga.limparBufferUART();
			uint8_t pacote[2] = {0x04, 0x81};
			size_t len = Protocolo::anexarCRC(pacote, 2);
			jiga.enviarBytesComoASCII(pacote, len);
			
			aguardandoResposta = true;
			timerUart = millis();
		}

		// B) Escuta a resposta
		if (aguardandoResposta)
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
					for (size_t i = 0; i < tamRaw; i += 2) {
						if (i + 1 >= tamRaw || idx >= 32) break;
						dados[idx++] = (Protocolo::charParaByte(bufRaw[i]) << 4) | Protocolo::charParaByte(bufRaw[i + 1]);
					}

					uint16_t vFonteRaw = (dados[7] << 8) | dados[8];
					uint16_t vBatRaw   = (dados[9] << 8) | dados[10];
					
					ultimaLeituraVin  = vFonteRaw / 1000.0; 
					ultimaLeituraVbat = vBatRaw / 1000.0;

					// Tolerância: Verifica erro na Entrada da Fonte
					if (abs(s.vLoad - ultimaLeituraVin) > 0.5)
					{
						char msgPlaca[21], msgJiga[21], linha3[21], linha4[21];
						dtostrf(ultimaLeituraVin, 4, 1, msgPlaca);
						dtostrf(s.vLoad, 4, 1, msgJiga);
						
						sprintf(linha3, "Placa le: %sV", msgPlaca);
						sprintf(linha4, "Jiga envia: %sV", msgJiga);

						jiga.desligarTudo();
						jiga.setPWM(255);
						jiga.mostrarTelaMensagem(F("FALHA: LEITURA PLACA"), F("Diferenca > 0.5V"), linha3, linha4);
						jiga.beepErro();
						delay(3000); 
						
						aguardandoResposta = false;
						flagEntradaEstado = false;
						estadoAtual = ST_REPROVADO;
						return; 
					}
				}
				aguardandoResposta = false; 
			}
			else if (millis() - timerUart > 500) 
			{
				aguardandoResposta = false; 
			}
		}
	}
	else
	{
		// No Passo 0 (LEDS APAGADOS), limpamos as variáveis para não mostrar lixo
		ultimaLeituraVin = 0.0;
		ultimaLeituraVbat = 0.0;
	}

	// ==========================================
	// 3. NOVO DASHBOARD DO LCD
	// ==========================================
	if (!flagEntradaEstado || passoLed != passoAnterior)
	{
		jiga.limparTela();
		jiga.mostrarTelaMensagem(listaMsgs[passoLed], "JIGA OUT:      V", "VIN:     BAT:     ", "DIR:SIM | ESC:NAO");
		passoAnterior = passoLed;
		flagEntradaEstado = true;
		tUpdateDisplay = 0;
	}

	if (millis() - tUpdateDisplay > 200)
	{
		char strJiga[6], strVin[6], strBat[6];
		
		dtostrf(s.vLoad, 4, 1, strJiga);
		
		jiga.atualizarParteTela(1, 10, strJiga); 

		// Só mostra os valores da placa se já estiver liberado (passo 1 em diante)
		if (passoLed >= 1) 
		{
			dtostrf(ultimaLeituraVin, 4, 1, strVin);
			dtostrf(ultimaLeituraVbat, 4, 1, strBat);
			jiga.atualizarParteTela(2, 4, strVin);   
			jiga.atualizarParteTela(2, 13, strBat);  
		} 
		else 
		{
			jiga.atualizarParteTela(2, 4, "--- ");
			jiga.atualizarParteTela(2, 13, "--- ");
		}

		tUpdateDisplay = millis();
	}

	// ==========================================
	// 4. NAVEGAÇÃO
	// ==========================================
	if (t == TECLA_ENTER)
	{
		jiga.beep();
		passoLed++;

		if (passoLed >= TOTAL_PASSOS_LED) {
			flagEntradaEstado = false;
			estadoAtual = ST_APROVADO;
		} else {
			flagEntradaEstado = false;
			estadoAtual = ST_LED_PREP;
		}
	}

	if (t == TECLA_ESC)
	{
		jiga.beepErro();
		passoLed = 0;
		flagEntradaEstado = false;
		estadoAtual = ST_REPROVADO;
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

// ==========================================
// SISTEMA DE SEGURANÇA CONTÍNUO (WATCHDOG)
// ==========================================
bool TestController::verificarCurtoCircuito(float alvo, float atual)
{
	// 1. Se o alvo mudou (nova etapa), reseta o timer
	if (alvo != alvoVoutAtual)
	{
		alvoVoutAtual = alvo;
		timerCurto = millis();
	}

	// 2. Se a tensão atual está a menos de 1.0V do alvo, a placa NÃO está em curto.
	// O timer fica sendo "zerado" para o tempo atual constantemente.
	if (abs(alvo - atual) <= 1.0)
	{
		timerCurto = millis();
		return false; // Tudo OK
	}

	// 3. Se a diferença for > 1.0V de forma contínua por mais de 2 segundos (2000ms)...
	if (millis() - timerCurto > 2000)
	{
		return true; // CURTO DETECTADO!
	}

	return false; // Ainda está tentando alcançar (dentro dos 2 segundos)
}

void TestController::dispararErroCurto()
{
	jiga.desligarTudo();
	jiga.setPWM(255); // Zera o PWM (Corta energia)

	// Mostra erro grave e reprova direto
	jiga.mostrarTelaMensagem(F("FALHA GRAVE: CURTO"), F("Tensao fora do alvo"), F("Placa Danificada!"), F("PRESSIONE BOTAO"));
	jiga.beepErro();
	delay(200);
	jiga.beepErro(); // Beep duplo para alertar o operador

	flagEntradaEstado = false;
	estadoAtual = ST_REPROVADO;
}