# Fix: falso positivo de "curto-circuito" no início do Teste AMS

**Data**: 2026-08-12
**Arquivo**: `src/TestController.cpp` — `TestController::amsTest`

## Sintoma

Ao selecionar "TESTE CCPS/CIR" no menu, o teste de detecção de AMS pirata reprovava a placa com "FALHA GRAVE: CURTO" logo na primeira etapa (rampa até 20V), mesmo com placas saudáveis.

## Causas identificadas

1. **Watchdog rodando sem a placa conectada**: em `AMS_RAMP_20V` o relé `Vbat` é mantido desligado de propósito (para gerar um degrau de tensão depois). Nessa etapa a fonte da própria jiga ainda está subindo de 0V a 20V sem nenhuma carga real conectada — não existe curto possível. Mesmo assim, `verificarCurtoCircuito` disparava caso a subida levasse mais de 2s (comum, já que é a maior variação de tensão do teste).
2. **Watchdog com estado "preso" entre execuções**: `verificarCurtoCircuito` só reseta seu cronômetro interno quando o valor-alvo (`alvo`) muda em relação à última chamada (`alvoVoutAtual`). Como o alvo da primeira etapa é sempre `V_AMS_CHECK` (20.0V), rodar o teste AMS uma segunda vez (mesma sessão) não resetava o cronômetro — o tempo "estourado" desde a rodada anterior permanecia válido, podendo reprovar a placa quase instantaneamente.

## Correção aplicada

- Removida a chamada a `verificarCurtoCircuito` durante `AMS_RAMP_20V` (placa ainda desconectada — nada a proteger nessa etapa).
- Adicionado reset explícito de `alvoVoutAtual`/`timerCurto` toda vez que o teste AMS é (re)iniciado (bloco `!flagEntradaEstado`), garantindo que cada execução comece com o watchdog "limpo".

## Ideia para próxima iteração

Avaliar generalizar esse reset para dentro de `verificarCurtoCircuito` (ex.: parâmetro opcional de reset, ou reset automático sempre que o estado que o chama muda), já que o mesmo padrão (`alvo` repetido entre execuções) pode se repetir em outras etapas que reusam a mesma tensão-alvo (`handlePowerCheck`, `handleLedPrep`, `handleLedTest`).
