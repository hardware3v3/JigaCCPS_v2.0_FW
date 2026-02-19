#ifndef TESTCONTROLLER_H
#define TESTCONTROLLER_H

#include "JigaHardware.h"

enum EstadoSistema {
    ST_MENU, 
    ST_MANUAL, 
    ST_AMS_TEST,      // <--- Novo Estado
    ST_INIT_TEST, 
    ST_COM_UART,
    ST_POWER_CHECK, 
    ST_POWER_TEST, 
    ST_LED_PREP, 
    ST_LED_TEST, 
    ST_APROVADO, 
    ST_REPROVADO
};

class TestController {
private:
    JigaHardware& jiga;
    EstadoSistema estadoAtual;
    
    // Variáveis de fluxo
    unsigned long timerEstado;
    int opcaoMenu;
    int passoLed;
    bool flagEntradaEstado;

    // Configuração de Testes
    static const int TOTAL_PASSOS_LED = 7;
    const float listaTensoes[7] = {
        V_TEST_UNDER, V_TEST_0_25, V_TEST_25_50, V_TEST_50_75, 
        V_TEST_75_100, V_TEST_OVER, V_TEST_CRITICAL
    };
    const char* listaMsgs[7] = {
        "LEDS APAGADOS?", "VM PISCA (0-25)?", "VM ACESO (25-50)?", 
        "VD PISCA (50-75)?", "VD ACESO (100%)?", "PISCA ALTERNADO?", "PISCA JUNTOS?"
    };

    // Handlers de Estado
    void handleMenu(Tecla t);
    void handleManual(Tecla t, LeituraSensores& s);
    void amsTest(Tecla t, LeituraSensores& s); // <--- Declaração do teste AMS
    void handleInitTest();
    void handleComUart();
    void handlePowerCheck(LeituraSensores& s);
    void handlePowerTest(Tecla t, LeituraSensores& s);
    void handleLedPrep(LeituraSensores& s);
    void handleLedTest(Tecla t, LeituraSensores& s);
    void handleAprovado(Tecla t);
    void handleReprovado(Tecla t);

    void resetarSistema();

public:
    TestController(JigaHardware& hw);
    void begin();
    void run();
};

#endif