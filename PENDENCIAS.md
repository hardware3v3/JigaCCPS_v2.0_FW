# Pendências para a próxima versão do firmware da jiga

## Saída da placa (vCCPS) não religa após o passo 3 do teste de LEDs

**Observado em:** 21/10/2026, branch `melhoria/ajuste-tensao-rapido`, com uma placa que foi aprovada.

**Sintoma:** depois do passo 3 (11,8 V, "LEDs apagados P2"), a saída da placa (`vCCPS`) cai para 0 V e não volta mais nos passos seguintes, nem a 14,4 V ou 18 V. A carga também não acende. Nos passos 1 e 2, logo depois do reset do DUT, a saída estava ligada (cerca de 11,7 V e 13 V).

**Telemetria (trecho):**
```
st=8 p=2 13,0V  vLoad=13.04 vCCPS=13.01   ← saída ligada
st=8 p=3 11,8V  vLoad=11.77 vCCPS=0.00    ← corta por subtensão
st=8 p=4 12,1V  vLoad=12.08 vCCPS=0.00
st=8 p=7 13,9V  vLoad=13.86 vCCPS=0.00
st=8 p=9 18,0V  vLoad=17.98 vCCPS=0.00    ← continua desligada
```

**Avaliar:**
- Confirmar no firmware do CCPS se o corte por subtensão deve travar até um novo boot ou se a saída deve religar quando a tensão sobe (histerese).
- Se deve religar, a jiga precisa verificar `vCCPS` nos passos acima do religamento e reprovar quando a saída não voltar. Hoje a jiga não verifica isso.
- Há uma validação de `vCCPS` comentada em `handleLedPrep` ("NOVA VALIDAÇÃO DA SAÍDA DA PLACA"), que foi desativada por causa de um bug no teste do CIR. Pode servir de ponto de partida.

**Status:** o teste foi aprovado assim por enquanto. Fica para a próxima versão.
