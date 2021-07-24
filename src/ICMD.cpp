//ICMD.cpp

#include <Arduino.h>
#include <CRC32.h>
#include <SPI.h>

void mdSetupTTL(SPIClass *spi, int csGPIO)
{
    //ICMD setup TTL
    spiWr(spi, 0x00, 0x01, csGPIO); //x2 24 bit counters, CNTR1 will be in bytes 0..2
                                    //                    CNTR0 will be in bytes 3..5
    spiWr(spi, 0x01, 0x81, csGPIO); //TTL Inputs, SPI IFC Priority
    spiWr(spi, 0x02, 0x00, csGPIO);
    spiWr(spi, 0x03, 0x00, csGPIO);
    spiWr(spi, 0x04, 0x04, csGPIO); //Disable BiSS CH0
    spiWr(spi, 0x30, 0x07, csGPIO); //Reset all counters
}

void mdSetup422(SPIClass *spi, int csGPIO)
{
    //ICMD RS422
    spiWr(spi, 0x00, 0x00, csGPIO); //24 bit
    spiWr(spi, 0x01, 0x01, csGPIO); //SPI IFC Priority
    spiWr(spi, 0x02, 0x00, csGPIO);
    spiWr(spi, 0x03, 0x00, csGPIO);
    spiWr(spi, 0x04, 0x04, csGPIO); //Disable BiSS CH0
    spiWr(spi, 0x30, 0x07, csGPIO); //Reset all counters
}

void spiWr(SPIClass *spi, int addr, int data, int csPin)
{
    digitalWrite(csPin, LOW);

    spi->transfer(addr);

    spi->transfer(data);

    digitalWrite(csPin, HIGH);
}