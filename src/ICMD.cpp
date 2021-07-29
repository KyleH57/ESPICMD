//ICMD.cpp

#include <Arduino.h>
#include <CRC32.h>
#include <SPI.h>

#include "ICMD.h"

// ICMD::ICMD(SPIClass *spi, int csGPIO, int config)
// {
//     //set Chip Select
//     CS = csGPIO;

//     //configure the IC using SPI writes
//     if (config)
//     {
//         mdSetup422(spi, CS);
//     }
//     else
//     {
//         mdSetupTTL(spi, CS);
//     }

//     // //initialize the byte buffer
//     // for (int i = 0; i < RDBUFFER_SIZE; i++)
//     // {
//     //     dataBuffer[i] = 0x0;
//     // }
// }
// uint32_t ICMD::getCounts(SPIClass *spi)
// {
//     const int readBytes = 3;
//     spiRd(spi, 0x08, readBytes, CS);

//     //set counts
//     uint32_t counts = (dataBuffer[0] << 16) | (dataBuffer[1] << 8) | (dataBuffer[2]);
//     //sign extension
//     counts <<= 8;
//     counts >> 8;

//     return counts;
// }
// // Read device ID to verify we can talk to decoder
// int ICMD::systemCheck(SPIClass *spi)
// {
//     //read 'M'
//     spiRd(spi, 0x78, 1, CS);

//     if (dataBuffer[0] == 'M')
//     {
//         return 0;
//     }
//     return 1;
// }




// void ICMD::mdSetupTTL(SPIClass *spi, int csGPIO)
// {
//     //ICMD setup TTL
//     spiWr(spi, 0x00, 0x00, csGPIO); //single 24 bit, can use 0x02 for single 48 bit counter.
//     spiWr(spi, 0x01, 0x81, csGPIO); //TTL Inputs, SPI IFC Priority
//     spiWr(spi, 0x02, 0x00, csGPIO);
//     spiWr(spi, 0x03, 0x00, csGPIO);
//     spiWr(spi, 0x04, 0x04, csGPIO); //Disable BiSS CH0
//     spiWr(spi, 0x30, 0x07, csGPIO); //Reset all counters
// }

// void ICMD::mdSetup422(SPIClass *spi, int csGPIO)
// {
//     //See page 25, table 61 for register map of counters
//     //update getCounts() to read 6 bytes if changing to 48 bit counter
//     //ICMD RS422
//     spiWr(spi, 0x00, 0x00, csGPIO); //single 24 bit, can use 0x02 for single 48 bit counter.
//     spiWr(spi, 0x01, 0x01, csGPIO); //SPI IFC Priority
//     spiWr(spi, 0x02, 0x00, csGPIO);
//     spiWr(spi, 0x03, 0x00, csGPIO);
//     spiWr(spi, 0x04, 0x04, csGPIO); //Disable BiSS CH0
//     spiWr(spi, 0x30, 0x07, csGPIO); //Reset all counters
// }

// void ICMD::spiRd(SPIClass *spi, int addr, int numBytes, int csPin)
// {

//     digitalWrite(csPin, LOW); //pull SS slow to prep other end for transfer

//     spi->transfer(0x80 | addr);

//     for (int i = 0; i < numBytes; i++)
//     {
//         dataBuffer[i] = spi->transfer(0x00);
//     }

//     digitalWrite(csPin, HIGH);
// }

// void ICMD::spiWr(SPIClass *spi, int addr, int data, int csPin)
// {
//     digitalWrite(csPin, LOW);

//     spi->transfer(addr);

//     spi->transfer(data);

//     digitalWrite(csPin, HIGH);
// }