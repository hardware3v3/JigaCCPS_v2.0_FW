#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <Arduino.h>

struct RespostaInterpretada {
    bool crcValido;
    uint8_t comando;
    uint8_t status;
};

class Protocolo {
public:
    // Utilitários Puros
    static uint16_t calcularCRC16(const uint8_t* data, size_t length);
    static size_t anexarCRC(uint8_t* buffer, size_t length);
    static bool verificarCRC(uint8_t* buffer, size_t length);
    
    // Helper reutilizável (Clean Code: DRY)
    static uint8_t charParaByte(char c);

    // Parser Principal
    static RespostaInterpretada interpretarPacote(uint8_t* buffer, size_t length);
};

#endif