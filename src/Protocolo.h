#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <Arduino.h>

// Estrutura para facilitar o retorno da análise
struct RespostaInterpretada {
    bool crcValido;
    uint8_t comando;
    uint8_t status;
    float valor; // Se houver
};

class Protocolo {
public:
    // --- Utilitários de CRC (Sua lógica inserida aqui) ---
    static uint16_t calcularCRC16(const uint8_t* data, size_t length);
    static size_t anexarCRC(uint8_t* buffer, size_t length);
    static bool verificarCRC(uint8_t* buffer, size_t length);
    

    // --- Parser ---
    static RespostaInterpretada interpretarPacote(uint8_t* buffer, size_t length);
};

#endif