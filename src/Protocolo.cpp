#include "Protocolo.h"

uint8_t Protocolo::charParaByte(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return 10 + (c - 'A');
	if (c >= 'a' && c <= 'f')
		return 10 + (c - 'a');
	return 0;
}

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
	uint8_t crcLow = buffer[length - 2];
	uint8_t crcHigh = buffer[length - 1];
	uint16_t crcRecebido = (crcHigh << 8) | crcLow;
	uint16_t crcCalculado = calcularCRC16(buffer, length - 2);
	return (crcCalculado == crcRecebido);
}

RespostaInterpretada Protocolo::interpretarPacote(uint8_t *bufferRaw, size_t lengthRaw)
{
	RespostaInterpretada resp = {false, 0, 0};
	uint8_t bufferConvertido[32];
	size_t lenConvertido = 0;

	// 1. Parser ASCII -> Binário
	for (size_t i = 0; i < lengthRaw; i += 2)
	{
		if (i + 1 >= lengthRaw)
			break;
		if (bufferRaw[i] == '\r' || bufferRaw[i] == '\n')
			break;

		uint8_t byteVal = (charParaByte(bufferRaw[i]) << 4) | charParaByte(bufferRaw[i + 1]);
		bufferConvertido[lenConvertido++] = byteVal;
	}

	// 2. Validação
	if (lenConvertido < 3)
		return resp;

	if (verificarCRC(bufferConvertido, lenConvertido))
	{
		resp.crcValido = true;
		if (lenConvertido >= 2)
		{
			resp.comando = bufferConvertido[0];
			resp.status = bufferConvertido[1];
		}
	}
	return resp;
}