#include <Arduino.h>
#include "JigaHardware.h"
#include "TestController.h"

// 1. Instanciar Hardware
JigaHardware hardware;

// 2. Instanciar Lógica de Teste (Injetando o hardware)
TestController controller(hardware);

void setup() {
    // Inicializa Hardware
    hardware.begin();
    
    // Inicializa Lógica
    controller.begin();
    
    hardware.beep(); // Check sonoro de boot
}

void loop() {
    // O loop apenas roda o "cérebro" da jiga
    controller.run();
}