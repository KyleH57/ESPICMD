#include <Arduino.h>
#include <CRC32.h>
#include <SPI.h>

// //Breadboard
#define VSPI_MISO 15
#define VSPI_MOSI 2
#define VSPI_SCLK 4
#define csXY 16 //bottom 2 encoders
#define csZ 17

static const int spiClk = 1000000; // 1 MHz

//uninitalised pointers to SPI objects
SPIClass *vspi = NULL;

uint8_t rdBuffer[9];

int countX = 0;
int countY = 0;
int countZ = 0;
void spiRd(int addr, int numB, int csPin);
void spiWr(int addr, int data, int csPin);

int chk = 0;
String payload = "";
String message = "";

// typedef struct message {
//   int messageLen;
//   int messageType;
//   int xCounts;
//   int yCounts;
//   int zCounts;
//   unsigned int sN;
//   uint32_t CRC;
// };

// message  msgDataBuffer;
// message *msgDataPtr = &msgDataBuffer;




void setup()
{
  //2 mil
  Serial.begin(115200);
  //initialise two instances of the SPIClass attached to VSPI and HSPI respectively
  vspi = new SPIClass(VSPI);

  //vspi->begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, csXY); //SCLK, MISO, MOSI, SS
  vspi->begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI); //SCLK, MISO, MOSI, SS
  vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  //set up slave select pins as outputs as the Arduino API
  //doesn't handle automatically pulling SS low
  pinMode(csXY, OUTPUT); //VSPI SS
  pinMode(csZ, OUTPUT);  //VSPI SS

  //ICMD setup XY
  spiWr(0x00, 0x01, csXY); //x2 24 bit counters
  spiWr(0x01, 0x81, csXY); //TTL Inputs, SPI IFC Priority
  spiWr(0x02, 0x00, csXY);
  spiWr(0x03, 0x00, csXY);
  spiWr(0x04, 0x04, csXY); //Disable BiSS CH0
  spiWr(0x30, 0x07, csXY); //Reset all counters

  //ICMD setup Z
  spiWr(0x00, 0x01, csZ); //x2 24 bit counters
  spiWr(0x01, 0x81, csZ); //TTL Inputs, SPI IFC Priority
  spiWr(0x02, 0x00, csZ);
  spiWr(0x03, 0x00, csZ);
  spiWr(0x04, 0x04, csZ); //Disable BiSS CH0
  spiWr(0x30, 0x07, csZ); //Reset all counters

  Serial.println("Setup done");
}



void loop()
{
  
  // //use the SPI buses

  
  spiRd(0x08, 6, csXY);

  //read in data MSB first and do sign extension
  countX = (rdBuffer[0] << 16) | (rdBuffer[1] << 8) | (rdBuffer[2]);
  countX <<= 8;
  countX >>= 8;

  countY = (rdBuffer[3] << 16) | (rdBuffer[4] << 8) | (rdBuffer[5]);
  countY <<= 8;
  countY >>= 8;

  spiRd(0x08, 3, csZ);
  countZ = (rdBuffer[3] << 16) | (rdBuffer[4] << 8) | (rdBuffer[5]);
  countZ <<= 8;
  countZ >>= 8;

  payload = String(countX) + "," + String(countY) + "," + String(countZ);

  chk = CRC32::calculate(&payload,sizeof(payload));

  message = String(chk) + "," + payload;

  Serial.flush();//wait for any existing data to be sent
  delay(4);
  Serial.println(message);

  
  // msgDataPtr->messageLen  = sizeof(message);
  // msgDataPtr->messageType = 1;
  // msgDataPtr->xCounts     = countX;
  // msgDataPtr->yCounts     = countY;
  // msgDataPtr->zCounts     = countZ;
  // msgDataPtr->sN          = 0;
  // msgDataPtr->CRC         = CRC32::calculate(msgDataPtr, msgDataPtr->messageLen-1 );

  //dereference and cast
  //Serial.write( (uint8_t *)msgDataPtr, sizeof(message) );


}

void spiRd(int addr, int numB, int csPin)
{

  digitalWrite(csPin, LOW); //pull SS slow to prep other end for transfer

  vspi->transfer(0x80 | addr);

  for (int i = 0; i < numB; i++)
  {
    rdBuffer[i] = vspi->transfer(0x00);
  }

  digitalWrite(csPin, HIGH);
}

void spiWr(int addr, int data, int csPin)
{
  digitalWrite(csPin, LOW);

  vspi->transfer(addr);

  vspi->transfer(data);

  digitalWrite(csPin, HIGH);
}