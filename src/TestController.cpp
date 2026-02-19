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
	case ST_COM_UART:
		handleComUart();
		break;
	case ST_POWER_CHECK:
		handlePowerCheck(s);
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
			jiga.escreverLinha(1, "> 1. TESTE CCPS");
		else
			jiga.escreverLinha(1, "  1. TESTE CCPS");

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

	// Seleção
	if (t == TECLA_ENTER)
	{
		jiga.beepSucesso();
		flagEntradaEstado = false;

		if (opcaoMenu == 0)
			estadoAtual = ST_AMS_TEST; // Teste AMS/Pirata
						   // estadoAtual = ST_INIT_TEST; // Teste Normal
		else if (opcaoMenu == 1)
			estadoAtual = ST_INIT_TEST; // Manual
		else if (opcaoMenu == 2)
			estadoAtual = ST_MANUAL; // Manual
						 // estadoAtual = ST_AMS_TEST; // Teste AMS/Pirata
						 //    estadoAtual = ST_INIT_TEST; // Teste Normal
	}
}

void TestController::amsTest(Tecla t, LeituraSensores &s)
{
	// Enum interno atualizado com erro de Curto
	enum SubEstadoAMS
	{
		AMS_INIT,
		AMS_RAMP_20V,
		AMS_CHECK_20V,
		AMS_RAMP_25V,
		AMS_HOLD_25V,
		AMS_PASS,
		AMS_FAIL_PIRATE, // Erro de tensão 3v3 (Pirata)
		AMS_FAIL_SHORT	 // Erro de Rampa (Curto/Sobrecarga)
	};

	static SubEstadoAMS subState = AMS_INIT;
	static unsigned long tTimer = 0; // Usado para Timeout da Rampa E para Contagem de 5s
	static unsigned long tUpdateDisplay = 0;
	static bool erroMostrado = false;

	// --- SETUP INICIAL ---
	if (!flagEntradaEstado)
	{
		jiga.desligarTudo();
		jiga.limparTela();
		jiga.mostrarTelaMensagem(F("TESTE DO AMS"), F("PREPARE-SE..."), F("Nao mexa na placa"), F(""));
		delay(1000);

		subState = AMS_INIT;
		erroMostrado = false;
		flagEntradaEstado = true;
	}

	// --- BOTÃO DE PÂNICO ---
	if (t == TECLA_ESC)
	{
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
		jiga.setVbat(true);	  // Habilita entrada de tensão
		jiga.controlarAlvo(true); // Habilita a placa (opcional, dependendo do seu circuito)

		tTimer = millis(); // <--- INICIA O CRONÔMETRO PARA A RAMPA DE 20V
		subState = AMS_RAMP_20V;
		break;

	case AMS_RAMP_20V:
		// 1. Atualiza Display
		if (millis() - tUpdateDisplay > 200)
		{
			char msg[21];
			sprintf(msg, "Alvo: 20V  At: %d.%d", (int)s.vLoad, (int)(s.vLoad * 10) % 10);
			jiga.mostrarTelaMensagem(F("ETAPA 1: SUBINDO"), F("Buscando 20V..."), msg, F(""));
			tUpdateDisplay = millis();
		}

		// 2. Proteção de Timeout (Curto-circuito)
		if (millis() - tTimer > 2000) // Se passar de 2 segundos tentando subir
		{
			subState = AMS_FAIL_SHORT; // ABORTA
			return;
		}

		// 3. Tenta Ajustar
		if (jiga.ajustarTensaoAutomatico(V_AMS_CHECK, s.vLoad))
		{
			// Chegou em 20V!
			tTimer = millis(); // Reseta timer para usar no próximo estado (Estabilização)
			subState = AMS_CHECK_20V;
		}
		break;

	case AMS_CHECK_20V:
		jiga.ajustarTensaoAutomatico(V_AMS_CHECK, s.vLoad);

		// Espera 1s estabilizar
		if (millis() - tTimer > 1000)
		{
			// Verifica o 3.3V
			if (s.v3v3 >= V_3V3_MIN && s.v3v3 <= V_3V3_MAX)
			{
				jiga.beepSucesso();
				tTimer = millis(); // <--- INICIA O CRONÔMETRO PARA A RAMPA DE 25V
				subState = AMS_RAMP_25V;
			}
			else
			{
				subState = AMS_FAIL_PIRATE;
			}
		}
		break;

	case AMS_RAMP_25V:
		// 1. Atualiza Display
		if (millis() - tUpdateDisplay > 200)
		{
			char msg[21];
			sprintf(msg, "Alvo: 25V  At: %d.%d", (int)s.vLoad, (int)(s.vLoad * 10) % 10);
			jiga.mostrarTelaMensagem(F("ETAPA 2: STRESS"), F("Subindo p/ 25V..."), msg, F("ATENCAO!"));
			tUpdateDisplay = millis();
		}

		// 2. Proteção de Timeout (Curto-circuito nos 25V) - AQUI É O MAIS IMPORTANTE
		if (millis() - tTimer > 2000)
		{
			subState = AMS_FAIL_SHORT; // ABORTA SE NÃO SUBIR EM 2s
			return;
		}

		// 3. Tenta Ajustar
		if (jiga.ajustarTensaoAutomatico(V_AMS_STRESS, s.vLoad))
		{
			// Chegou em 25V!
			tTimer = millis(); // Inicia timer dos 5 segundos de hold
			subState = AMS_HOLD_25V;
		}
		break;

	case AMS_HOLD_25V:
		jiga.ajustarTensaoAutomatico(V_AMS_STRESS, s.vLoad);

		// Monitoramento contínuo da saída 3V3
		if (s.v3v3 < V_3V3_MIN || s.v3v3 > V_3V3_MAX)
		{
			subState = AMS_FAIL_PIRATE;
			return;
		}

		if (millis() - tUpdateDisplay > 200)
		{
			int restantes = 5 - (millis() - tTimer) / 1000;
			char msg[21];
			sprintf(msg, "Segure: %ds", restantes);
			char msgV33[21];
			dtostrf(s.v3v3, 4, 2, msgV33);
			char msgLinha4[21];
			sprintf(msgLinha4, "3V3: %sV (OK)", msgV33);
			jiga.mostrarTelaMensagem(F("TESTE DESTRUTIVO"), F("Aplicando 25V..."), msg, msgLinha4);
			tUpdateDisplay = millis();
		}

		if (millis() - tTimer > 5000)
		{
			subState = AMS_PASS;
		}
		break;

	case AMS_PASS:
		jiga.desligarTudo();
		jiga.setPWM(255);
		static bool flagMusic = true;

		if (millis() - tUpdateDisplay > 1000)
		{

			jiga.mostrarTelaMensagem(F("AMS ORIGINAL"), F("APROVADO (25V OK)"), F("Regulador Robusto"), F("ENTER p/ Continuar"));

			if (flagMusic)
			{
				jiga.tocarMelodiaVitoria();
				flagMusic = false;
			}

			tUpdateDisplay = millis();
		}

		if (t == TECLA_ENTER)
		{
			flagMusic = true;
			flagEntradaEstado = false;
			estadoAtual = ST_INIT_TEST;
		}
		break;

	// --- CASO 1: TENSÃO SUBIU, MAS REGULADOR FALHOU (PIRATA) ---
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

	// --- CASO 2: TENSÃO NEM SUBIU (CURTO-CIRCUITO / SOBRECARGA) ---
	case AMS_FAIL_SHORT:
		if (!erroMostrado)
		{
			// DESLIGA TUDO IMEDIATAMENTE
			jiga.desligarTudo();
			jiga.setPWM(255);

			// Mensagem específica de Curto
			jiga.mostrarTelaMensagem(F("FALHA GRAVE: CURTO"), F("Tensao nao subiu!"), F("Verifique a placa"), F("PRESSIONE BOTAO"));

			// Beep longo e contínuo para alertar
			jiga.beepErro();
			delay(200);
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
		char l1[21], l2[21], l3[21];
		sprintf(l1, "MANUAL PWM:%d", jiga.getPWM());
		char vLoadStr[6], vCapStr[6];
		dtostrf(s.vLoad, 4, 1, vLoadStr);
		dtostrf(s.vCap, 4, 1, vCapStr);

		sprintf(l2, "V_OUT: %sV %s", vLoadStr, cargaLigada ? "ON " : "OFF");
		sprintf(l3, "V_CAP: %sV", vCapStr);

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
		// jiga.setVbat(false);
		// jiga.setCarga(true);
		jiga.setVbat(true);
		jiga.setCarga(false);
		jiga.limparBufferUART();
		timerEstado = millis();
		enviado = false;
		flagEntradaEstado = true;
	}

	if (!enviado && millis() - timerEstado > 1000)
	{
		uint8_t pacote[10];
		pacote[0] = 0x04;
		pacote[1] = 0x81;
		size_t len = Protocolo::anexarCRC(pacote, 2);
		jiga.enviarBytesComoASCII(pacote, len);
		enviado = true;
		timerEstado = millis();
	}

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

				// Conversão Hex -> Byte usando Helper do Protocolo
				for (size_t i = 0; i < tamRaw; i += 2)
				{
					if (i + 1 >= tamRaw || idx >= 32)
						break;
					uint8_t h = Protocolo::charParaByte(bufRaw[i]);
					uint8_t l = Protocolo::charParaByte(bufRaw[i + 1]);
					dados[idx++] = (h << 4) | l;
				}

				uint8_t fwVersion = dados[2];
				uint16_t vFonte = (dados[7] << 8) | dados[8];
				uint16_t vBat = (dados[9] << 8) | dados[10];

				if (fwVersion == 0x04 && (resp.status == 0x81 || resp.status == 0x15))
				{
					char msgFonte[21], msgBat[21];
					sprintf(msgFonte, "V.Fonte: %d", vFonte);
					sprintf(msgBat, "V.Bat:   %d", vBat);
					jiga.mostrarTelaMensagem(F("PLACA COMPATIVEL"), F("FW: v04 Detectado"), msgFonte, msgBat);

					jiga.beepSucesso();
					delay(3000);
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

	char lido[10], msg[21];
	dtostrf(s.vLoad, 4, 1, lido);
	sprintf(msg, "LIDO: %s V", lido);

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
	static unsigned long tUpdateDisplay = 0;

	if (!flagEntradaEstado)
	{
		timerEstado = millis();
		tUpdateDisplay = 0;
		jiga.limparTela();
		jiga.mostrarTelaMensagem(F("2. ALIMENTACAO"), F("LED ENTRADA ACESO?"), F("VCap:      V"), F("ESC:REPRO ENTER:APR"));
		jiga.desligarTudo();
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

	if (s.vCap < 5.0 && (millis() - timerEstado > 2000))
	{
		jiga.setCarga(false);
		jiga.mostrarTelaMensagem(F("FALHA: CAPACITOR"), F("Tensao baixa"), F(""), F(""));
		jiga.beepErro();
		estadoAtual = ST_REPROVADO;
	}

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

	// Quando a Jiga finalmente estabilizar na tensão alvo:
	if (jiga.ajustarTensaoAutomatico(listaTensoes[passoLed], s.vLoad))
	{
		// --- NOVA VALIDAÇÃO DA SAÍDA DA PLACA ---
		// Índices do passoLed:
		// 0 = "LEDS APAGADOS?"
		// 1 = "VM PISCA (0-25)?"
		// 2 = "VM ACESO (25-50)?"  <-- A partir daqui faremos a checagem
		if (passoLed >= 3 && passoLed <=5)
		{
			Serial.println("Estou no if");
			Serial.println(s.vCCPS);
			// Verifica se a saída do dispositivo (vCCPS) está fora da faixa 11V - 14V
			if (s.vCCPS < 11.0 || s.vCCPS > 16.0)
			{
				// Formata o valor lido para mostrar na tela o motivo do erro
				char lido[10];
				dtostrf(s.vCCPS, 4, 1, lido);
				char msgErro[21];
				sprintf(msgErro, "Lido: %s V", lido);

				// Reprova imediatamente
				jiga.desligarTudo();
				jiga.mostrarTelaMensagem(F("FALHA: SAIDA PLACA"), F("Esp: 11.0V a 14.0V"), msgErro, F("PRESSIONE BOTAO"));
				jiga.beepErro();

				flagEntradaEstado = false;

				estadoAtual = ST_REPROVADO;
				return; // Aborta a função para não ir para o ST_LED_TEST
			}
		}
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

	jiga.ajustarTensaoAutomatico(listaTensoes[passoLed], s.vLoad);

	if (!flagEntradaEstado || passoLed != passoAnterior)
	{
		jiga.limparTela();
		jiga.mostrarTelaMensagem(F("3. TESTE LEDS"), listaMsgs[passoLed], "V-JIGA:      V", "DIR >> CONFIRMA");
		passoAnterior = passoLed;
		flagEntradaEstado = true;
		tUpdateDisplay = 0;
	}

	if (millis() - tUpdateDisplay > 200)
	{
		char atualStr[10];
		dtostrf(s.vLoad, 4, 2, atualStr);
		jiga.atualizarParteTela(2, 8, atualStr);
		tUpdateDisplay = millis();
	}

	if (t == TECLA_ENTER)
	{
		jiga.beep();
		passoLed++;

		if (passoLed >= TOTAL_PASSOS_LED)
		{
			flagEntradaEstado = false;
			estadoAtual = ST_APROVADO;
		}
		else
		{
			// --- A CORREÇÃO ESTÁ AQUI ---
			// Antigamente só tinha um delay(200); aqui.
			// Precisamos mandar ele de volta para a tela de "Aguarde..." (Prep)
			// para que ele suba a tensão e caia no seu IF!

			flagEntradaEstado = false;
			estadoAtual = ST_LED_PREP;
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