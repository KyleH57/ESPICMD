#include <Arduino.h>
#include <CRC32.h>
#include <SPI.h>

void mdSetupTTL(SPIClass *spi, int csGPIO);

void mdSetup422(SPIClass *spi, int csGPIO);

void spiWr(SPIClass *spi, int addr, int data, int csPin);