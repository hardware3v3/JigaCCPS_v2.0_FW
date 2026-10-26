# Proposta: corrigir `fresh_start` global e generalizar reset do watchdog de curto

**Data**: 2026-08-12
**Status**: PROPOSTA — aguardando aprovação, nada implementado ainda.
**Arquivo afetado**: `src/TestController.cpp`

---

## Problema 1: `fresh_start` só funciona nas duas primeiras placas testadas

### Onde está
```cpp
// TestController.cpp, linha 940 (fora da classe, escopo de arquivo)
static uint8_t fresh_start = 2;

void TestController::handleLedPrep(LeituraSensores &s)
{
    if (fresh_start) {
        jiga.desligarTudo();
        delay(1000);
        digitalWrite(PIN_DUT_RST, LOW);
        delay(1000);
        digitalWrite(PIN_DUT_RST, HIGH);
        delay(1000);
    }
    ...
    if (jiga.ajustarTensaoAutomatico(...)) {
        if (fresh_start)
            fresh_start--;
        ...
```

### Por que é um bug
`fresh_start` é inicializado uma única vez, no boot do Arduino (`static ... = 2`), e nunca mais volta a esse valor. O pulso de reset no DUT (`PIN_DUT_RST`) — que existe para garantir que a placa sob teste ligue "limpa" antes da etapa de LEDs — só roda:

- 1ª placa testada após ligar a jiga: reseta (fresh_start 2→1)
- 2ª placa: reseta de novo (fresh_start 1→0)
- 3ª placa em diante, **pelo resto do turno**: nunca mais reseta, porque `fresh_start` fica em 0 permanentemente.

Isso é silencioso — não gera erro nem aviso na tela. O operador não tem como saber que a placa não está sendo resetada antes do teste de LED.

### Correção proposta
Trocar a variável `static` de arquivo (persiste entre placas) por uma flag de instância que é religada toda vez que um novo ciclo de teste começa — ou seja, no ponto em que o fluxo entra em `ST_INIT_TEST` (que já representa "começando um novo teste do zero", chamado tanto do Menu quanto de Aprovado/Reprovado).

```cpp
// TestController.h — adicionar ao private:
bool precisaResetarDUT;

// TestController.cpp — handleInitTest()
void TestController::handleInitTest()
{
    jiga.desligarTudo();
    jiga.setPWM(150);
    jiga.mostrarTelaMensagem(F("INICIANDO TESTES..."), F("Aguarde..."), F(""), F(""));
    delay(1000);
    precisaResetarDUT = true;   // <-- NOVO: toda placa nova começa precisando reset
    flagEntradaEstado = false;
    estadoAtual = ST_COM_UART;
}

// handleLedPrep() — troca "fresh_start" por "precisaResetarDUT"
void TestController::handleLedPrep(LeituraSensores &s)
{
    if (precisaResetarDUT) {
        jiga.desligarTudo();
        delay(1000);
        digitalWrite(PIN_DUT_RST, LOW);
        delay(1000);
        digitalWrite(PIN_DUT_RST, HIGH);
        delay(1000);
    }
    jiga.setVbat(true);
    ...
    if (jiga.ajustarTensaoAutomatico(listaTensoes[passoLed], s.vLoad))
    {
        if (precisaResetarDUT)
            precisaResetarDUT = false;
        ...
```

Também remover a variável `static uint8_t fresh_start = 2;` de escopo de arquivo (linha 940) e inicializar `precisaResetarDUT = false;` no construtor de `TestController`.

### Efeito esperado
Toda placa nova testada recebe o pulso de reset antes da etapa de LED, não só as duas primeiras do turno.

---

## Problema 2: watchdog de curto (`verificarCurtoCircuito`) reusa estado entre etapas com o mesmo alvo

### Onde está
```cpp
// TestController.cpp, linhas ~1119-1143
bool TestController::verificarCurtoCircuito(float alvo, float atual)
{
    if (alvo != alvoVoutAtual)       // só reseta se o ALVO mudou
    {
        alvoVoutAtual = alvo;
        timerCurto = millis();
    }
    if (abs(alvo - atual) <= 1.0) { timerCurto = millis(); return false; }
    if (millis() - timerCurto > 2000) return true;
    return false;
}
```

`alvoVoutAtual` e `timerCurto` são variáveis de instância únicas, compartilhadas por **todas** as chamadas em todos os estados: `amsTest`, `handlePowerCheck`, `handleLedPrep`, `handleLedTest`. Já corrigi o caso do teste AMS (reset explícito ao reentrar no estado). Mas o mesmo padrão de bug pode se repetir em qualquer lugar que reuse um alvo de tensão já visitado antes:

- `listaTensoes` (LEDs) repete valores: índices 1 e 3 usam `V_TEST_0_FRESH_BOOT`. Se o operador volta a esse alvo, o timer pode já estar "velho" e a placa reprovar por curto quase na hora.
- `handlePowerCheck` sempre usa alvo fixo (`12.0`). Se o teste for reiniciado (nova placa) mantendo o mesmo alvo, o timer não reseta ao reentrar no estado — só reseta se o alvo numericamente mudar.

### Correção proposta
Generalizar o reset para acontecer sempre que **o estado é reentrado** (`!flagEntradaEstado`), e não só quando o valor do alvo muda. Adicionar um método auxiliar:

```cpp
// TestController.h
void resetarWatchdogCurto();

// TestController.cpp
void TestController::resetarWatchdogCurto()
{
    alvoVoutAtual = -1.0f;   // valor impossível, força reset na próxima chamada
    timerCurto = millis();
}
```

E chamar `resetarWatchdogCurto()` no bloco de entrada (`if (!flagEntradaEstado) { ... }`) de cada handler que usa `verificarCurtoCircuito`:
- `handlePowerCheck`
- `handleLedPrep` (hoje não tem bloco de entrada próprio — precisaria adicionar um, controlado por `passoLed == 0` ou por uma flag equivalente à `precisaResetarDUT`)
- `handleLedTest` (já tem `flagEntradaEstado` verificado)

Isso deixa o watchdog do curto **por-entrada-de-estado**, igual ao que já fizemos manualmente no `amsTest`, mas de forma reutilizável — sem precisar repetir a lógica em cada handler.

### Efeito esperado
Reprovação por "curto" passa a refletir sempre uma condição real da placa atual, nunca um resquício de tempo de uma etapa ou placa anterior.

---

## O que NÃO está incluído nesta proposta
- Adição de `TECLA_ESC` em todos os estados de teste (mencionado na conversa anterior) — fica para uma proposta separada, é uma mudança de UX/fluxo, não uma correção de bug silencioso.
- Qualquer mudança no algoritmo de `ajustarTensaoAutomatico` (malha aberta) — considerado aceitável como está.

## Pergunta para aprovação
Concorda com as duas correções acima? Se sim, implemento os dois pontos em `TestController.h`/`.cpp` e recompilo para validar.
