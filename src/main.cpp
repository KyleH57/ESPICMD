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


static const int MSG_BUFFER_HEADER          = 0x7FBEDEAD;      //Msg Header
static const int MSG_BUFFER_SEPARATOR       = ',';             //Msg Separator char  0x2C

static const int MSG_BUFFER_HEADER_SIZE     = (1 * (8 + 1));   //32 bit value w/separator char
static const int MSG_BUFFER_SEQ_CNT_SIZE    = (1 * (8 + 1));   //32 bit value w/separator char
static const int MSG_BUFFER_ENC_COUNTS_SIZE = (3 * (6 + 1));   //3 24 bit values w/separator after each value
static const int MSG_BUFFER_CRC_SIZE        = (1 * (8 + 0));   //32 bit CRC - 8 charachters, last field so no separators

static const int MSG_BUFFER_MAX_SIZE        = MSG_BUFFER_HEADER_SIZE     +
                                              MSG_BUFFER_SEQ_CNT_SIZE    +
                                              MSG_BUFFER_ENC_COUNTS_SIZE +
                                              MSG_BUFFER_CRC_SIZE        +
                                              8                          ;  //Extra Space

uint8_t msgBuffer[MSG_BUFFER_MAX_SIZE];
int     msgSequenceCount = 0;

uint8_t binToHexTbl[] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46};

int      i, j;
int      msgIndex;
int      tempValue;



int countX = 0;
int countY = 0;
int countZ = 0;
void spiRd(int addr, int numB, int csPin);
void spiWr(int addr, int data, int csPin);

uint32_t chk = 0;
String payload = "";
String message = "";
String test = "";


/*******************************************************************************
*
*  dbgPrintMsgBuffer()
*
66 67 
51 65 
49 50 
42 42
 44

B C 3 A 1 2 * *

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
int formatValueToAsciiHexStr(int value, int startIndex, boolean addDelimiter)
{
uint8_t *tempPtr;
int      nibbleLO, nibbleHI;
int      i, j;
boolean  nonZeroFound = false;

  i       = startIndex;
  tempPtr = (uint8_t *)&value;
  tempPtr = tempPtr + 3;

  /*****************************************************************************
  * Convert each nibble to its Ascii equivalent.  ESP32 is Little Endian so 
  * process nibbles from the right
  *****************************************************************************/
  for (j = 0; j < sizeof(int); j++)
  {
    nibbleLO =  *tempPtr       & 0x0F;
    nibbleHI = (*tempPtr >> 4) & 0x0F;

    if (nibbleHI != 0x00) {
       nonZeroFound = true;
       msgBuffer[i++] = binToHexTbl[(nibbleHI)];
    } else {
      if (nonZeroFound == true) {
        msgBuffer[i++] = binToHexTbl[(nibbleHI)];
      }
    }

    if (nibbleLO != 0x00) {
       nonZeroFound = true;
       msgBuffer[i++] = binToHexTbl[(nibbleLO)];
    } else {
      if (nonZeroFound == true) {
        msgBuffer[i++] = binToHexTbl[(nibbleLO)];
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
  //msgIndex = formatValueToAsciiHexStr(MSG_BUFFER_HEADER, msgIndex, true);
  msgIndex = formatValueToAsciiHexStr(msgSequenceCount,  msgIndex, true);
  msgIndex = formatValueToAsciiHexStr(countX,            msgIndex, true);
  msgIndex = formatValueToAsciiHexStr(countY,            msgIndex, true);
  msgIndex = formatValueToAsciiHexStr(countZ,            msgIndex, true);

  chk = CRC32::calculate(msgBuffer, msgIndex);

  msgIndex = formatValueToAsciiHexStr(chk, msgIndex, false);

  /****************************************************************************
  * Send Message (Currently as a String)
  ****************************************************************************/
  Serial.println( (char *)msgBuffer );
//  Serial.print  (" : MsgSize = ");            //Debug
//  Serial.println( msgIndex );                 //Debug

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