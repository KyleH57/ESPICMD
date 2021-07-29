// #ifndef INC_ICMD_H_
// #define INC_ICMD_H_


#ifndef ICMD_H
#define ICMD_H
//#pragma once
#include <Arduino.h>
#include <CRC32.h>
#include <SPI.h>

class ICMD
{
public:

    
    
    ICMD(int csGPIO, int config);

    void spiSetup(SPIClass *spi);

    uint32_t getCounts(SPIClass *spi);

    int systemCheck(SPIClass *spi);

private:
    int CS;
    int icConfig;
    static const int RDBUFFER_SIZE = 16;
    uint8_t dataBuffer[RDBUFFER_SIZE];



    void mdSetupTTL(SPIClass *spi, int csGPIO);

    void mdSetup422(SPIClass *spi, int csGPIO);

    void spiRd(SPIClass *spi, int addr, int numB, int csPin);

    void spiWr(SPIClass *spi, int addr, int data, int csPin);
};

#endif