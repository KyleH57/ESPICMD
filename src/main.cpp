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


static const int MSG_TYPE_ASCII_HEX_STR     = 0x00;            //CSV List of parameters, Leading '0's removed, 
                                                               //  data formatted as ASCII HEX w/out 0x prefix
static const int MSG_TYPE_BINARY_V1         = 0x01;            //FUTURE:  Msg Struct sent as binary data

static const int msgType = MSG_TYPE_ASCII_HEX_STR;

static const int MSG_BUFFER_SEPARATOR       = ',';             //Msg Separator char  0x2C

static const int MSG_BUFFER_MSG_LENGTH_SIZE = (1 * (4 + 1));   //16 bit value w/separator char

static const int MSG_BUFFER_MSG_TYPE_SIZE   = (1 * (8 + 1));   //32 bit value w/separator char
static const int MSG_BUFFER_SEQ_CNT_SIZE    = (1 * (8 + 1));   //32 bit value w/separator char
static const int MSG_BUFFER_ENC_COUNTS_SIZE = (3 * (6 + 1));   //3 24 bit values w/separator after each value
static const int MSG_BUFFER_CRC_SIZE        = (1 * (8 + 0));   //32 bit CRC - 8 charachters, last field so no separators

static const int MSG_BUFFER_MAX_SIZE        = MSG_BUFFER_MSG_TYPE_SIZE   +
                                              MSG_BUFFER_SEQ_CNT_SIZE    +
                                              MSG_BUFFER_ENC_COUNTS_SIZE +
                                              MSG_BUFFER_CRC_SIZE        +
                                              8                          ;  //Extra Space

uint8_t msgLengthBuffer[MSG_BUFFER_MSG_LENGTH_SIZE];
uint8_t msgBuffer      [MSG_BUFFER_MAX_SIZE];
int     msgSequenceCount = 0;
int     msgIndex;
int     payloadLength;        //CRC Will be calculated over these bytes


uint8_t binToHexTbl[] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46};

static const boolean REMOVE_LEADING_ZEROES     = true;
static const boolean LEAVE_LEADING_ZEROES      = false;

static const boolean ADD_DELIMITER    = true;
static const boolean NO_DELIMITER     = true;



int countX = 0;
int countY = 0;
int countZ = 0;

void spiRd(int addr, int numB, int csPin);
void spiWr(int addr, int data, int csPin);

uint32_t chk = 0;


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
     Serial.print( (char)msgBuffer[i] );
     Serial.print(" ");
  }

}   //End of dbgPrintMsgBuffer()



/*******************************************************************************
*
*  formatValueToAsciiHexStr()
*
  Serial.println("\n-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=");
  Serial.print  ("startIndex = ");
  Serial.println(i);
    Serial.print  (j);
    Serial.print  (" : HI = ");
    Serial.print  (nibbleHI);
    Serial.print  (" : LO = ");
    Serial.println(nibbleLO);
*
*******************************************************************************/
int formatValueToAsciiHexStr(uint8_t *bufferPtr, int value, int startIndex, boolean removeLeadingZeroes, boolean addDelimiter)
{
uint8_t *tempPtr;
int      tempValue;
int      nibbleLO, nibbleHI;
int      i, j;
boolean  nonZeroFound = false;

  i         = startIndex;
  tempValue = value;


  /*****************************************************************************
  * Convert each nibble to its Ascii equivalent.  ESP32 is Little Endian so 
  * process nibbles from the right
  *****************************************************************************/
  tempPtr   = (uint8_t *)&tempValue;
  tempPtr   = tempPtr + 3;

  for (j = 0; j < sizeof(int); j++)
  {
    nibbleLO =  *tempPtr       & 0x0F;
    nibbleHI = (*tempPtr >> 4) & 0x0F;

    if (removeLeadingZeroes == false) {
       *bufferPtr++ = binToHexTbl[(nibbleHI)];
       *bufferPtr++ = binToHexTbl[(nibbleLO)];        
    }
    else {
      // Don't Store any leading zeroes...
       if (nibbleHI != 0x00) {
          nonZeroFound = true;
          *bufferPtr++ = binToHexTbl[(nibbleHI)];
      } else {
          if (nonZeroFound == true) {
             *bufferPtr++ = binToHexTbl[(nibbleHI)];
          }
      }

      if (nibbleLO != 0x00) {
         nonZeroFound = true;
         *bufferPtr++ = binToHexTbl[(nibbleLO)];
      } else {
         if (nonZeroFound == true) {
            *bufferPtr++ = binToHexTbl[(nibbleLO)];
         }
      }

    }

    tempPtr--;  //Go to next byte

  }   //End of for (j = 0; j < sizeof(int); j++)

  /*****************************************************************************
  * If value being processed is 0, then nothing will have been stored in  
  * msgBuffer.  We should return something, so add in '0' to msgBuffer
  *****************************************************************************/
  if (nonZeroFound == false) {
    msgBuffer[i++] = binToHexTbl[0x00];
  }

  /****************************************************************************
  * Add Delimiter if requested
  ****************************************************************************/
  if (addDelimiter == true) {
    msgBuffer[i++] = MSG_BUFFER_SEPARATOR;
  }

  msgBuffer[i] = 0x00;    //Null Terminate so we can process as a string

  //Serial.println( (char *)msgBuffer );     //Debug

  return i;    //Return current buffer index

}   // End of formatAsciiHexStr()


/*******************************************************************************
*
*  setup()
*
*******************************************************************************/
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

  /****************************************************************************
  * Format Msg To Send
  ****************************************************************************/
  msgIndex = 0;
  msgIndex = formatValueToAsciiHexStr(msgBuffer, msgType,           msgIndex, REMOVE_LEADING_ZEROES, ADD_DELIMITER);
  msgIndex = formatValueToAsciiHexStr(msgBuffer, msgSequenceCount,  msgIndex, REMOVE_LEADING_ZEROES, ADD_DELIMITER);
  msgIndex = formatValueToAsciiHexStr(msgBuffer, countX,            msgIndex, REMOVE_LEADING_ZEROES, ADD_DELIMITER);
  msgIndex = formatValueToAsciiHexStr(msgBuffer, countY,            msgIndex, REMOVE_LEADING_ZEROES, ADD_DELIMITER);
  msgIndex = formatValueToAsciiHexStr(msgBuffer, countZ,            msgIndex, REMOVE_LEADING_ZEROES, ADD_DELIMITER);

  payloadLength = msgIndex;

  chk = CRC32::calculate(msgBuffer, payloadLength);

  msgIndex = formatValueToAsciiHexStr(msgBuffer, chk,               msgIndex, REMOVE_LEADING_ZEROES, NO_DELIMITER);


  msgIndex = formatValueToAsciiHexStr(msgLengthBuffer, payloadLength, 0, REMOVE_LEADING_ZEROES, ADD_DELIMITER);


  /****************************************************************************
  * Send Message (Currently as a String)
  ****************************************************************************/
  Serial.print  ( (char *)msgLengthBuffer);
  Serial.println( (char *)msgBuffer      );

  delay(5);
  
  Serial.flush();   //wait for any existing data to be sent

  msgSequenceCount++;

}   //End of Loop()



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