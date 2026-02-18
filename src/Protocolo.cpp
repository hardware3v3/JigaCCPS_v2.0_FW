#include "Protocolo.h"

// --- AUXILIAR: Converte caractere ASCII ('0'-'F') para valor numérico (0-15) ---
uint8_t charParaByte(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return 10 + (c - 'A');
	if (c >= 'a' && c <= 'f')
		return 10 + (c - 'a');
	return 0;
}

// --- IMPLEMENTAÇÃO DO CRC (MANTIDA) ---
uint16_t Protocolo::calcularCRC16(const uint8_t *data, size_t length)
{
	uint16_t crc = 0x0000;
	uint16_t polynomial = 0xA001;

	for (size_t i = 0; i < length; i++)
	{
		crc ^= data[i];
		for (uint8_t j = 0; j < 8; j++)
		{
			if (crc & 1)
			{
				crc = (crc >> 1) ^ polynomial;
			}
			else
			{
				crc >>= 1;
			}
		}
	}
	return crc;
}

size_t Protocolo::anexarCRC(uint8_t *buffer, size_t length)
{
	uint16_t crc = calcularCRC16(buffer, length);
	// Little Endian
	buffer[length] = (uint8_t)(crc & 0xFF);
	buffer[length + 1] = (uint8_t)((crc >> 8) & 0xFF);
	return length + 2;
}

bool Protocolo::verificarCRC(uint8_t *buffer, size_t length)
{
	if (length < 3)
		return false;

	uint8_t crcLowRecebido = buffer[length - 2];
	uint8_t crcHighRecebido = buffer[length - 1];
	uint16_t crcRecebido = (crcHighRecebido << 8) | crcLowRecebido;

	uint16_t crcCalculado = calcularCRC16(buffer, length - 2);
	return (crcCalculado == crcRecebido);
}

// --- Interpretador com Conversão ASCII -> BINÁRIO ---
RespostaInterpretada Protocolo::interpretarPacote(uint8_t *bufferRaw, size_t lengthRaw)
{
	RespostaInterpretada resp;
	resp.crcValido = false;
	resp.comando = 0;
	resp.status = 0;

	// Buffer temporário para os bytes reais convertidos
	uint8_t bufferConvertido[32];
	size_t lenConvertido = 0;

	// 1. CONVERSÃO (Parser ASCII Hex -> Byte)
	// Percorre o bufferRaw de 2 em 2 caracteres
	for (size_t i = 0; i < lengthRaw; i += 2)
	{

		// Pula caracteres de quebra de linha (\n, \r) se houver no final
		if (bufferRaw[i] == '\r' || bufferRaw[i] == '\n')
			break;
		if (i + 1 >= lengthRaw)
			break; // Evita ler fora do array se for ímpar

		char charHigh = (char)bufferRaw[i];
		char charLow = (char)bufferRaw[i + 1];

		// Se encontrou quebra de linha no meio, para
		if (charHigh == '\r' || charHigh == '\n')
			break;

		// Junta "0" e "5" -> 0x05
		uint8_t valorByte = (charParaByte(charHigh) << 4) | charParaByte(charLow);

		bufferConvertido[lenConvertido++] = valorByte;
	}

	// 2. VALIDAÇÃO (Agora usamos o bufferConvertido)
	if (lenConvertido < 3)
	{
		return resp; // Muito curto
	}

	if (!verificarCRC(bufferConvertido, lenConvertido))
	{
		return resp; // Erro de CRC
	}

	// Se chegou aqui, o CRC bateu!
	resp.crcValido = true;

	// Extrai dados do buffer limpo
	// RX Real: 05 15 F2 ...
	// Byte 0: Comando (05)
	// Byte 1: Status (15)
	if (lenConvertido >= 2)
	{
		resp.comando = bufferConvertido[0];
		resp.status = bufferConvertido[1];
	}

	return resp;
}