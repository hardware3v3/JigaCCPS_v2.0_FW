# Jiga CCPS v2.0 — Descrição do Firmware

## O que é

Firmware para uma bancada de testes (jiga) baseada em Arduino Nano (ATmega328), usada para validar placas CCPS/CIR na linha de produção/reparo. A jiga simula a alimentação da placa sob teste (DUT), lê suas saídas analógicas e se comunica com ela via UART para validar hardware e firmware antes da placa ser liberada.

## Hardware controlado

- **Fonte ajustável**: saída de tensão controlada por PWM (`PIN_PWM_CTRL`), com malha de ajuste manual em software (`ajustarTensaoAutomatico`). PWM alto = tensão baixa, PWM baixo = tensão alta (regulador inversamente proporcional).
- **Relé Vbat** (`VBAT_ON_PIN`): conecta/desconecta a fonte ajustável da placa sob teste — usado para aplicar "degraus" de tensão controlados.
- **Relé de carga** (`LOAD_ON_PIN`): aplica carga na saída da placa.
- **Reset do DUT** (`PIN_DUT_RST`): permite resetar/ligar a placa sob teste remotamente.
- **UART por software** (`SoftwareSerial`, 19200 baud): protocolo binário próprio com CRC (ver `Protocolo.cpp`) para trocar dados com o firmware da placa.
- **Leituras analógicas**: tensão de saída da carga (`vLoad`), saída do CCPS (`vCCPS`), tensão no capacitor (`vCap`) e trilho de 3V3/5V do regulador AMS (`v3v3`).
- **Display LCD I2C 20x4** e **teclado** de 4 botões (cima/baixo/enter/esc) para interface do operador.
- **Buzzer** para feedback sonoro (sucesso, erro, melodias).

## Fluxo de testes (máquina de estados em `TestController`)

1. **Menu inicial**: operador escolhe entre "Teste CCPS/CIR" (com verificação de AMS), "Teste ECO/DIR" (sem CCPS) ou "Modo Manual".
2. **Teste AMS** (`ST_AMS_TEST`, só na opção CCPS/CIR): rampa a fonte até 20V com a placa desconectada, conecta a placa (degrau), verifica se o regulador interno (AMS) entrega 3.3V (placa padrão) ou 5V (placa CIR) dentro da tolerância. Se aprovado, sobe até 25V (teste de estresse/destrutivo) e mantém por 5s monitorando a saída regulada. Se o regulador não aguenta ou nunca regula corretamente, a placa é reprovada como "AMS pirata" (componente não original, incapaz de sustentar sobretensão).
3. **Init Test**: desliga tudo, prepara PWM para a próxima etapa.
4. **Comunicação UART**: envia pacote binário e espera resposta da placa, valida versão de firmware e CRC.
5. **Power Check / Power Test**: ajusta a fonte para 12V e verifica capacitor/alimentação de entrada da placa.
6. **Preparo e teste de LEDs**: percorre 10 patamares de tensão (`listaTensoes`) para verificar visualmente o comportamento dos LEDs indicadores da placa em cada faixa de bateria (crítico, baixo, médio, cheio, sobretensão etc.), com confirmação manual do operador a cada passo.
7. **Aprovado/Reprovado**: tela final com resultado, som de vitória/derrota, e opção de reiniciar o ciclo.

## Segurança

Um watchdog de curto-circuito (`verificarCurtoCircuito`) monitora continuamente se a tensão de saída consegue acompanhar o alvo definido; se ficar mais de 1.0V fora do alvo por mais de 2 segundos, a jiga desliga tudo e reprova a placa por "curto/sobrecarga" (`dispararErroCurto`). Há também um reset de emergência acionado ao pressionar os 4 botões simultaneamente.
