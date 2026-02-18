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

	// 2. Segurança Global
	if (jiga.checarResetEmergencia())
	{
		resetarSistema();
	}

	// 3. Máquina de Estados
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

// --- IMPLEMENTAÇÃO DOS ESTADOS ---

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

		// Aqui são variáveis RAM (buffers), então OK usar a função padrão
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

	// 3. RECEPÇÃO
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

				// Função lambda definida antes do loop
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
						break; // Trava de segurança

					char h = bufRaw[i];
					char l = bufRaw[i + 1];
					dados[idx++] = (hex2bin(h) << 4) | hex2bin(l);
				}

				// Extração dos dados
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

				// --- AQUI ESTÁ A ALTERAÇÃO PARA MOSTRAR TENSÕES ---

				// Se quiser aceitar OUTRAS versões, mude para: if (fwVersion >= 0x04 ...)
				if (fwVersion == 0x04 && (resp.status == 0x81 || resp.status == 0x15))
				{
					char msgFonte[21];
					char msgBat[21];

					// Formata para exibir (Ex: "V.Fonte: 1250")
					sprintf(msgFonte, "V.Fonte: %d", tensaoFonteRaw);
					sprintf(msgBat, "V.Bat:   %d", tensaoBatRaw);

					// Exibe tudo no LCD usando a função Híbrida (Flash, Flash, RAM, RAM)
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

	// Aqui misturamos Flash com RAM, usamos a versão Híbrida
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
	// Timer para não atualizar o número rápido demais (ilegível)
	static unsigned long tUpdateDisplay = 0;

	// --- 1. DESENHA O TEMPLATE (Só uma vez ao entrar) ---
	if (!flagEntradaEstado)
	{
		timerEstado = millis();
		tUpdateDisplay = 0;

		jiga.limparTela();
		// Desenhamos tudo o que é FIXO agora.
		// Note os espaços em branco na linha 2 para encaixar o número depois
		// Linha 0: "2. ALIMENTACAO"
		// Linha 1: "LED POWER ACESO?"
		// Linha 2: "VCap:      V"  <-- O número vai entrar depois do "VCap: "
		// Linha 3: "DIR >> CONFIRMA"
		jiga.mostrarTelaMensagem(
		    F("2. ALIMENTACAO"),
		    F("LED POWER ACESO?"),
		    F("VCap:      V"),
		    F("DIR >> CONFIRMA"));

		flagEntradaEstado = true;
	}

	// --- 2. ATUALIZA SÓ O NÚMERO (A cada 200ms) ---
	if (millis() - tUpdateDisplay > 200)
	{
		char vCapStr[10];
		// Formata o float. 4 digitos total, 1 decimal (ex: "12.5")
		dtostrf(s.vCap, 4, 1, vCapStr);

		// Escreve APENAS o número na Linha 2, Coluna 6 (Logo após "VCap: ")
		jiga.atualizarParteTela(2, 6, vCapStr);

		tUpdateDisplay = millis();
	}

	// --- 3. LÓGICA DE TESTE (Roda na velocidade máxima) ---

	// Validação Automática do Capacitor (Monitoramento contínuo)
	if (s.vCap < 5.0 && (millis() - timerEstado > 2000))
	{
		jiga.setCarga(false);
		jiga.mostrarTelaMensagem(F("FALHA: CAPACITOR"), F("Tensao baixa"), F(""), F(""));
		jiga.beepErro();
		estadoAtual = ST_REPROVADO;
	}

	// Validação Visual pelo Operador
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
    // Controle de tempo para atualização do display
    static unsigned long tUpdateDisplay = 0;
    // Precisamos saber se o passo mudou para redesenhar a instrução (Ex: "VM PISCA" -> "VM ACESO")
    static int passoAnterior = -1;

    // --- 1. CONTROLE DE TENSÃO (Prioridade Máxima - Roda sempre) ---
    // Mantém o ajuste fino rodando a cada milissegundo para a tensão não cair
    jiga.ajustarTensaoAutomatico(listaTensoes[passoLed], s.vLoad);

    // --- 2. DESENHO DO TEMPLATE (Só se mudou o passo ou entrou no estado) ---
    if (!flagEntradaEstado || passoLed != passoAnterior)
    {
        jiga.limparTela();
        
        // Desenhamos a estrutura fixa.
        // Linha 0: "3. TESTE LEDS"
        // Linha 1: Variável (Ex: "VM PISCA (0-25)?")
        // Linha 2: "V-JIGA:      V" (Deixamos espaço para o número)
        // Linha 3: "DIR >> CONFIRMA"
        
        // Nota: listaMsgs[] é um array de char*, então combina com a função híbrida (F, RAM, RAM, RAM)
        jiga.mostrarTelaMensagem(
            F("3. TESTE LEDS"), 
            listaMsgs[passoLed], 
            "V-JIGA:      V", 
            "DIR >> CONFIRMA"
        );

        passoAnterior = passoLed;
        flagEntradaEstado = true;
        tUpdateDisplay = 0; // Força atualização imediata do número
    }

    // --- 3. ATUALIZAÇÃO DO VALOR (A cada 200ms) ---
    if (millis() - tUpdateDisplay > 200)
    {
        char atualStr[10];
        dtostrf(s.vLoad, 4, 2, atualStr); // Formata com 2 casas decimais
        
        // Escreve apenas o número na linha 2, coluna 8 (Logo após "V-JIGA: ")
        jiga.atualizarParteTela(2, 8, atualStr);
        
        tUpdateDisplay = millis();
    }

    // --- 4. INTERAÇÃO ---
    if (t == TECLA_ENTER)
    {
        jiga.beep();
        passoLed++;
        
        if (passoLed >= TOTAL_PASSOS_LED)
        {
            flagEntradaEstado = false; // Reset para o próximo estado
            estadoAtual = ST_APROVADO;
        }
        else
        {
            // Não mudamos o estadoAtual (continuamos no ST_LED_TEST),
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