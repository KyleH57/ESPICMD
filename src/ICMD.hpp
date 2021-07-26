#ifndef ICMD_H
#define ICMD_H

#include <Arduino.h>
#include <CRC32.h>
#include <SPI.h>

class ICMD
{
public:
    int gpioCS;
    static const int RDBUFFER_SIZE;
    ICMD(SPIClass *spi, int csGPIO, int config);

    uint32_t getCounts();

    int systemCheck();

private:
    void mdSetupTTL(SPIClass *spi, int csGPIO);

    void mdSetup422(SPIClass *spi, int csGPIO);

    void spiRd(int addr, int numB, int csPin);

    void spiWr(SPIClass *spi, int addr, int data, int csPin);
};

#endif