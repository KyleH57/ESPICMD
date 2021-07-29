#include <Arduino.h>
#include <CRC32.h>
#include <SPI.h>

#include "ICMD.h"

//black PCB
#define VSPI_MISO 15
#define VSPI_MOSI 2
#define VSPI_SCLK 4
#define csX 16 //X
#define csY 17 //Y
#define csZ 36 //Z

#define MD_CONFIG_RS422 0
#define MD_CONFIG_TTL 1

static const int spiClk = 1000000; // 1 MHz

//uninitalised pointers to SPI objects
SPIClass *vspi = NULL;

static const int RDBUFFER_SIZE = 16;
uint8_t rdBuffer[RDBUFFER_SIZE];

static const int MSG_TYPE_BINARY_V1 = 0x01;     //FUTURE:  Msg Struct sent as binary data
static const int MSG_TYPE_ASCII_HEX_STR = 0x0F; //CSV List of parameters, Leading '0's removed,
                                                //  data formatted as ASCII HEX w/out 0x prefix

static const int msgType = MSG_TYPE_ASCII_HEX_STR;

static const int MSG_BUFFER_SEPARATOR = ','; //Msg Separator char  0x2C

static const int MSG_BUFFER_MSG_LENGTH_SIZE = (1 * (4 + 1)); //16 bit value w/separator char

static const int MSG_BUFFER_MSG_TYPE_SIZE = (1 * (8 + 1));   //32 bit value w/separator char
static const int MSG_BUFFER_SEQ_CNT_SIZE = (1 * (8 + 1));    //32 bit value w/separator char
static const int MSG_BUFFER_ENC_COUNTS_SIZE = (3 * (6 + 1)); //3 24 bit values w/separator after each value
static const int MSG_BUFFER_CRC_SIZE = (1 * (8 + 0));        //32 bit CRC - 8 charachters, last field so no separators

static const int MSG_BUFFER_MAX_SIZE = MSG_BUFFER_MSG_TYPE_SIZE +
                                       MSG_BUFFER_SEQ_CNT_SIZE +
                                       MSG_BUFFER_ENC_COUNTS_SIZE +
                                       MSG_BUFFER_CRC_SIZE +
                                       8; //Extra Space

uint8_t msgLengthBuffer[MSG_BUFFER_MSG_LENGTH_SIZE];
uint8_t msgBuffer[MSG_BUFFER_MAX_SIZE];
uint32_t msgSequenceCount = 0;
uint32_t msgCRC = 0;
int msgIndex;
int payloadLength; //CRC Will be calculated over these bytes

uint8_t binToHexTbl[] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46};

static const boolean REMOVE_LEADING_ZEROES = true;
static const boolean LEAVE_LEADING_ZEROES = false;

static const boolean ADD_DELIMITER = true;
static const boolean NO_DELIMITER = false;

//setup ICMD objects
ICMD mdX(csX, MD_CONFIG_TTL);
ICMD mdY(csY, MD_CONFIG_TTL);
ICMD mdZ(csZ, 1);

int countX = 0;
int countY = 0;
int countZ = 0;

void spiRd(int addr, int numB, int csPin);
void spiWr(int addr, int data, int csPin);

/*******************************************************************************
*
*  dbgPrintMsgBuffer()
*
*******************************************************************************/
void dbgPrintMsgBuffer(int startIndex, int numChars)
{
  int i;

  for (i = startIndex; i < numChars; i++)
  {
    Serial.print((char)msgBuffer[i]);
    Serial.print(" ");
  }

} //End of dbgPrintMsgBuffer()

/*******************************************************************************
*
*  formatValueToAsciiHexStr()
*
*******************************************************************************/
int formatValueToAsciiHexStr(uint8_t *bufferPtr, int value, int startIndex, boolean removeLeadingZeroes,
                             boolean addDelimiter)
{
  uint8_t *valuePtr;
  uint8_t *wrPtr;
  int tempValue;
  int nibbleLO, nibbleHI;
  int j;
  boolean nonZeroFound = false;

  wrPtr = bufferPtr + startIndex;
  tempValue = value;

  /*****************************************************************************
  * Convert each nibble to its Ascii equivalent.  ESP32 is Little Endian so 
  * process nibbles from the right (Bit 0)
  *****************************************************************************/
  valuePtr = (uint8_t *)&tempValue;
  valuePtr = valuePtr + 3;

  for (j = 0; j < sizeof(int); j++)
  {
    nibbleLO = *valuePtr & 0x0F;
    nibbleHI = (*valuePtr >> 4) & 0x0F;

    if (removeLeadingZeroes == false)
    {
      *wrPtr++ = binToHexTbl[(nibbleHI)];
      *wrPtr++ = binToHexTbl[(nibbleLO)];
    }
    else
    {
      //
      // removeLeadingZeroes is TRUE - Don't Store any leading zeroes...
      //
      if (nibbleHI != 0x00)
      {
        nonZeroFound = true;
        *wrPtr++ = binToHexTbl[(nibbleHI)];
      }
      else
      {
        if (nonZeroFound == true)
        {
          *wrPtr++ = binToHexTbl[(nibbleHI)];
        }
      }

      if (nibbleLO != 0x00)
      {
        nonZeroFound = true;
        *wrPtr++ = binToHexTbl[(nibbleLO)];
      }
      else
      {
        if (nonZeroFound == true)
        {
          *wrPtr++ = binToHexTbl[(nibbleLO)];
        }
      }
    }

    valuePtr--; //Go to next byte

  } //End of for (j = 0; j < sizeof(int); j++)

  /*****************************************************************************
  * If value being processed is 0, then nothing will have been stored in  
  * msgBuffer.  We should return something, so add in '0' to msgBuffer
  *****************************************************************************/
  if (nonZeroFound == false)
  {
    *wrPtr++ = binToHexTbl[0x00];
  }

  /****************************************************************************
  * Add Delimiter if requested
  ****************************************************************************/
  if (addDelimiter == true)
  {
    *wrPtr++ = MSG_BUFFER_SEPARATOR;
  }

  *wrPtr = 0x00; //Null Terminate so we can process as a string

  //Serial.println( (char *)bufferPtr );     //Debug

  return (wrPtr - bufferPtr); //Return # of bytes stored

} // End of formatAsciiHexStr()

/*******************************************************************************
*
*  setup()
*
*******************************************************************************/
void setup()
{
  int i;

  //2 mil
  Serial.begin(115200);

  //initialize buffer
  for (i = 0; i < RDBUFFER_SIZE; i++)
  {
    rdBuffer[i] = 0x00;
  }

  //initialise two instances of the SPIClass attached to VSPI and HSPI respectively
  vspi = new SPIClass(VSPI);

  //vspi->begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, csXY); //SCLK, MISO, MOSI, SS

  vspi->begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI); //SCLK, MISO, MOSI, SS
  vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));

//do spi stuff now that SPI is running
  mdX.spiSetup(vspi);
  mdY.spiSetup(vspi);
  mdZ.spiSetup(vspi);
  //set up slave select pins as outputs as the Arduino API
  //doesn't handle automatically pulling SS low
  pinMode(csX, OUTPUT); //VSPI SS
  pinMode(csY, OUTPUT); //VSPI SS
  pinMode(csZ, OUTPUT); //VSPI SS

  //print handshake stat
  Serial.println(mdX.systemCheck(vspi));
  Serial.println(mdY.systemCheck(vspi));
  Serial.println(mdZ.systemCheck(vspi));


  Serial.println("Setup done");

  //setup done
  for (;;)
  {
    if (Serial.readStringUntil('\n') == "spin")
    {
      Serial.println("ENC");
      break;
    }
    delay(800);
  }
}

/*******************************************************************************
*
*  loop()
*
*******************************************************************************/
void loop()
{

  // countX = mdX.getCounts(vspi);
  // countY = mdY.getCounts(vspi);
  // countZ = mdZ.getCounts(vspi);

  /****************************************************************************
  * Format Msg To Send
  ****************************************************************************/
  msgIndex = 0;
  msgIndex = formatValueToAsciiHexStr(msgBuffer, msgType, msgIndex, REMOVE_LEADING_ZEROES, ADD_DELIMITER);
  msgIndex = formatValueToAsciiHexStr(msgBuffer, msgSequenceCount, msgIndex, REMOVE_LEADING_ZEROES, ADD_DELIMITER);
  msgIndex = formatValueToAsciiHexStr(msgBuffer, countX, msgIndex, REMOVE_LEADING_ZEROES, ADD_DELIMITER);
  msgIndex = formatValueToAsciiHexStr(msgBuffer, countY, msgIndex, REMOVE_LEADING_ZEROES, ADD_DELIMITER);
  msgIndex = formatValueToAsciiHexStr(msgBuffer, countZ, msgIndex, REMOVE_LEADING_ZEROES, ADD_DELIMITER);

  payloadLength = msgIndex;

  msgCRC = CRC32::calculate(msgBuffer, payloadLength);

  msgIndex = formatValueToAsciiHexStr(msgBuffer, msgCRC, msgIndex, REMOVE_LEADING_ZEROES, NO_DELIMITER);

  // Since payloadLength string will be variable, we'll create separate string value
  msgIndex = formatValueToAsciiHexStr(msgLengthBuffer, payloadLength, 0, REMOVE_LEADING_ZEROES, ADD_DELIMITER);

  /****************************************************************************
  * Send Message (Payload Length first w/Delimiter + remainder of message)
  ****************************************************************************/
  Serial.print((char *)msgLengthBuffer);
  Serial.println((char *)msgBuffer);

  msgSequenceCount++;

  delay(5);

  Serial.flush(); //wait for any existing data to be sent

} //End of Loop()

/*******************************************************************************
*
*  spiRd()
*  spiWr()
*
*******************************************************************************/
// void spiRd(int addr, int numB, int csPin)
// {

//   digitalWrite(csPin, LOW); //pull SS slow to prep other end for transfer

//   vspi->transfer(0x80 | addr);

//   for (int i = 0; i < numB; i++)
//   {
//     rdBuffer[i] = vspi->transfer(0x00);
//   }

//   digitalWrite(csPin, HIGH);
// }

// void spiWr(int addr, int data, int csPin)
// {
//   digitalWrite(csPin, LOW);

//   vspi->transfer(addr);

//   vspi->transfer(data);

//   digitalWrite(csPin, HIGH);
// }