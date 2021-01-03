//*****************************************************************************
//
// Apple //c Smartport Compact Flash adapter
// Written by Robert Justice  email: rjustice(at)internode.on.net
// Ported to Arduino UNO with SD Card adapter by Andrea Ottaviani email: andrea.ottaviani.69(at)gmail.com
//
// 1.00 - basic read and write working for one partition
// 1.01 - add support for 4 partions, assume no other smartport devices on bus
// 1.02 - add correct handling of device ids, should work with other smartport devices
//        before this one in the drive chain.
// 1.03 - fixup smartort bus enable and reset handling, will now start ok with llc reset
// 1.04 - fixup rddata line handling so that it will work with internal 5.25 drive. Now if
//        there is a disk in the floppy drive, it will boot first. Otherwise it will boot
//        from the CF
// 1.05 - fix problem with internal disk being always write protected. Code was stuck in 
//        receivepacket loop, so wrprot(ACK) was always 1. Added timeout support for
//        receivepacket function, so this now times out and goes back to the main loop for
//        another try.
// 1.1  - added support for being connected after unidisk 3.5. Need some more io pins to 
//        support pass through, this is the next step.
// 1.12 - add eeprom storing of boot partition, cycling by eject button (PA3)
// 1.13 - Fixed an issue where the block number was calculated incorrectly, leading to
//        failed operations. Now Total Replay v3 works!
// 1.14 - Added OLED support to display info, remove 6 second bootup delay, code rework.
//
// Apple disk interface is connected as follows:
// wrprot = pa5 (ack) (output)
// ph0    = pd2 (req) (input)
// ph1    = pd3       (input)
// ph2    = pd4       (input)
// ph3    = pd5       (input)
// rddata = pd6       (output from avr)
// wrdata = pd7       (input to avr)
//
//led i/o = pa4  (for led on when i/o on boxed version) 
//eject button = pa3  (for boxed version, cycle between boot partitions) 
// 
// OLED connected to spare digital ports 8 & 9.
//
// Serial port was connected for debug purposes. Most of the prints have been commented out.
// I left these in and these can be uncommented as required for debugging. Sometimes the
// prints add to much delay, so you need to be carefull when adding these in.
//
// NOTE: This is uses the ata code from the fat/fat32/ata drivers written by
//       Angelo Bannack and Giordano Bruno Wolaniuk and ported to mega32 by Murray Horn.
//
//*****************************************************************************

#include <SPI.h>
#include "Sd2Card.h"
#include <EEPROM.h>
#include <U8x8lib.h>  //Need to install U8g2 lib in library manager

// Set USE_SDIO to zero for SPI card access. 
#define USE_SDIO 0

#include <avr/io.h>
#include <stdio.h>
#include <avr/pgmspace.h>

#define PORT_REQ    PORTD   // Define the PORT to REQ
#define PIN_REQ     2     // Define the PIN number to REQ
#define PORT_ACK    PORTC   // Define the PORT to ACK
#define PIN_ACK     5     // Define the PIN number to ACK

#define NUM_PARTITIONS  4          // Number of 32MB Prodos partions supported

#define DEFINE 0

void print_hd_info(void);
void encode_data_packet (unsigned char source);   //encode smartport 512 byte data packet
int  decode_data_packet (void);                   //decode smartport 512 byte data packet
void encode_write_status_packet(unsigned char source,unsigned char status);
void encode_init_reply_packet (unsigned char source, unsigned char status);
void encode_status_reply_packet (unsigned char source);
void print_packet (unsigned char* data, int bytes);
int  packet_length (void);
int partition;
 
extern "C" unsigned char ReceivePacket(unsigned char*); //Receive smartport packet assembler function
extern "C" unsigned char SendPacket(unsigned char*);    //send smartport packet assembler function

unsigned char packet_buffer[768];   //smartport packet buffer
unsigned char sector_buffer[512];   //ata sector data buffer
unsigned char status,packet_byte;
int count;
int initPartition;
unsigned char device_id[NUM_PARTITIONS];     //to hold assigned device id's for the partitions

//The circuit:
//    SD card attached to SPI bus as follows:
// ** MOSI - pin 11 on Arduino Uno/Duemilanove/Diecimila
// ** MISO - pin 12 on Arduino Uno/Duemilanove/Diecimila
// ** CLK - pin 13 on Arduino Uno/Duemilanove/Diecimila
// ** CS - depends on your SD card shield or module.
//     Pin 10 used here 

// Change the value of chipSelect if your hardware does
// not use the default value, SS.  Common values are:
// Arduino Ethernet shield: pin 4
// Sparkfun SD shield: pin 8
// Adafruit SD shields and modules: pin 10

const uint8_t chipSelect = 10;
const uint8_t ejectPin = 17;
const uint8_t statusledPin =18;

const uint8_t OLEDsck =8;
const uint8_t OLEDsda =9;



// Set USE_SDIO to zero for SPI card access. 
//
// Initialize at highest supported speed not over 50 MHz.
// Reduce max speed if errors occur.

/*
 * Set DISABLE_CHIP_SELECT to disable a second SPI device.
 * For example, with the Ethernet shield, set DISABLE_CHIP_SELECT
 * to 10 to disable the Ethernet controller.
 */
const int8_t DISABLE_CHIP_SELECT = 0;  // -1

//
// Pin numbers in templates must be constants.

#if USE_SDIO
  // Use faster SdioCardEX
  SdioCardEX sd;
#else  // USE_SDIO
  Sd2Card sd;
#endif  // USE_SDIO

//Define OLED
U8X8_SSD1306_128X32_UNIVISION_SW_I2C u8x8(/* clock=*/ OLEDsck, /* data=*/ OLEDsda, /* reset=*/ U8X8_PIN_NONE);

// Strings
#include <avr/pgmspace.h>
const char string_0[] PROGMEM = "SmartPortSDv1.14";
const char string_1[] PROGMEM = "Partition:";
const char string_2[] PROGMEM = "Eject Pressed!";
const char string_3[] PROGMEM = "Card Init Error!";
const char string_4[] PROGMEM = "Error!";
const char string_5[] PROGMEM = "";
const char* const string_table[] PROGMEM = {string_0, string_1, string_2, string_3, string_4, string_5};
char  chrdata[16];
//------------------------------------------------------------------------------

void setup() {
  //Light status LED to measure startup time.
  digitalWrite(statusledPin,HIGH);

  // put your setup code here, to run once:
  mcuInit();

  pinMode(ejectPin,INPUT);
  pinMode(OLEDsck,OUTPUT);
  pinMode(OLEDsda,OUTPUT);

  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFont(u8x8_font_chroma48medium8_r);

 

  Serial.begin(9600);
  Serial.print(F("\r\nSmartportSD v1.14\r\n"));

  initPartition = EEPROM.read(0);

  if (initPartition == 0xFF) initPartition = 0;
  initPartition = (initPartition % 4);

  strcpy_P(chrdata, (char*)pgm_read_word(&(string_table[0])));  //"SmartPortSDv1.14"
  u8x8.drawString(0,0,chrdata);
  strcpy_P(chrdata, (char*)pgm_read_word(&(string_table[1]))); //"Partition:"
  u8x8.drawString(0,1,chrdata);
  itoa(initPartition,chrdata,16);
  u8x8.drawString(12,1,chrdata);
  
  Serial.print(F("\r\nBoot partition: "));
  Serial.print(initPartition,DEC);

  if (init_SD_card())
    while(1);

  digitalWrite(statusledPin,LOW);
}

//*****************************************************************************
// Function: main loop
// Parameters: none
// Returns: 0
//
// Description: Main function for Apple //c Smartport Compact Flash adpater
//*****************************************************************************


void loop() {
  // put your main code here, to run repeatedly:

  unsigned long int block_num;
  unsigned char LBH,LBL,LBN;

  int number_partitions_initialised=1;
  int noid=0;
  int count;
  bool sdstato;
  unsigned char source,status,phases,status_code;
  //Serial.print(F("\r\nloop"));
  DDRD=0x00;

  PORTD &= ~(_BV(6));    // set RD low
  interrupts();
  while(1) {

    if (digitalRead(ejectPin) == HIGH)
      rotate_boot();

    noid=0;    //reset noid flag
    DDRC=0xDF; //set ack (wrprot) to input to avoid clashing with other devices when sp bus is not enabled

    // read phase lines to check for smartport reset or enable
    phases = (PIND & 0x3c) >> 2;

    //Serial.print(phases);
    switch (phases) {
      // phase lines for smartport bus reset
      // ph3=0 ph2=1 ph1=0 ph0=1
      case 0x05:
      Serial.print(F("\r\nReset\r\n"));
      //Serial.print(number_partitions_initialised);
      // monitor phase lines for reset to clear
      while((PIND & 0x3c) >> 2 == 0x05);
      number_partitions_initialised=1;  //reset number of partitions init'd
      noid = 0;   // to check if needed 
      for (partition=0;partition<NUM_PARTITIONS;partition++) //clear device_id table
      device_id[partition] = 0;
      break;

      // phase lines for smartport bus enable
      // ph3=1 ph2=x ph1=1 ph0=x
      case 0x0a:
      case 0x0b:
      case 0x0e:
      case 0x0f:
      //Serial.print(F("E ")); //this is timing sensitive, so can't print to much here as it takes to long
      noInterrupts();
      DDRC=0xFF;     //set ack to output, sp bus is enabled
      if ((status = ReceivePacket( (unsigned char*) packet_buffer))) {
        interrupts();
        break;     //error timeout, break and loop again 
      }
      interrupts();

      // lets check if the pkt is for us
      if (packet_buffer[14] != 0x85)  // if its an init pkt, then assume its for us and continue on
      {  
        // else check if its our one of our id's
        for  (partition=0;partition < NUM_PARTITIONS;partition++)
        {
          if ( device_id[partition] != packet_buffer[6])  //destination id
            noid++;
        }
        if (noid == NUM_PARTITIONS)  //not one of our id's
        {
          //printf_P(PSTR("\r\nnot ours\r\n") );
          DDRC=0xDF; //set ack to input, so lets not interfere
          PORTC &= ~(_BV(5));   //set ack low, for next time its an output
          while(PINC & 0x20);   //wait till low other dev has finished receiving it
          //printf_P(PSTR("a ") );
          //print_packet ((unsigned char*) packet_buffer,packet_length());


          //assume its a cmd packet, cmd code is in byte 14
          //now we need to work out what type of packet and stay out of the way
          switch (packet_buffer[14]) {
            case 0x80:  //is a status cmd
            case 0x83:  //is a format cmd
            case 0x81:  //is a readblock cmd
              while(!(PINC & 0x20));    //wait till high
              //Serial.print(("A ") );
              while(PINC & 0x20);       //wait till low
              //Serial.print(("a ") );
              while(!(PINC & 0x20));   //wait till high
              //Serial.print(("A\r\n") );
              break;
            case 0x82:  //is a writeblock cmd
              while(!(PINC & 0x20));    //wait till high
              //Serial.print(("W ") );
              while(PINC & 0x20);       //wait till low
              //Serial.print(("w ") );
              while(!(PINC & 0x20));    //wait till high
              //Serial.print(("W\r\n") );
              while(PINC & 0x20);       //wait till low
              //Serial.print(("w ") );
              while(!(PINC & 0x20));    //wait till high
              //Serial.print(("W\r\n") );
              break;
          }
          break;  //not one of ours     
        }
      }

      //else it is ours, we need to handshake the packet
      PORTC &= ~(_BV(5));   //set ack low
      while(PIND & 0x04);    //wait for req to go low
      //assume its a cmd packet, cmd code is in byte 14
      //Serial.print("\r\nCMD:");
      //Serial.print(packet_buffer[14],HEX);
      //print_packet ((unsigned char*) packet_buffer,packet_length());

      switch (packet_buffer[14]) {

        case 0x80:  //is a status cmd
          digitalWrite(statusledPin,HIGH);
          source = packet_buffer[6];
          for (partition=0;partition < NUM_PARTITIONS;partition++) { //Check if its one of ours
            if (device_id[partition] == source) {  //yes it is, then reply
              //Added (unsigned short) cast to ensure calculated block is not underflowing.
              status_code = (packet_buffer[17] & 0x7f) | (((unsigned short)packet_buffer[16] << 3) & 0x80);
              if (status_code == 0x03){  // if statcode=3, then status with device info block
                encode_status_dib_reply_packet(source);
              } else {  // else just return device status
                encode_status_reply_packet(source);
              }
              noInterrupts();
              DDRD=0x40; //set rd as output
              status = SendPacket( (unsigned char*) packet_buffer);
              DDRD=0x00; //set rd back to input so back to tristate
              interrupts();
              //printf_P(PSTR("\r\nSent Packet Data\r\n") ); //legacy debugging stuff?
              //print_packet ((unsigned char*) packet_buffer,packet_length());
              //Serial.print("\r\nStatus CMD");
              digitalWrite(statusledPin,LOW);
            }
          }
          break;

        case 0x81:  //is a readblock cmd
          source = packet_buffer[6];
          //Serial.print("\r\nDrive ");
          //Serial.print(source,HEX);
          LBH = packet_buffer[16];
          LBL = packet_buffer[20];
          LBN = packet_buffer[19];
          for (partition=0;partition < NUM_PARTITIONS;partition++) { //Check if its one of ours
            if (device_id[partition] == source) {  //yes it is, then do the read
              // block num 1st byte
              //Added (unsigned short) cast to ensure calculated block is not underflowing.
              block_num = (LBN & 0x7f) | (((unsigned short)LBH << 3) & 0x80);
              // block num second byte
              //print_packet ((unsigned char*) packet_buffer,packet_length());
              //Added (unsigned short) cast to ensure calculated block is not underflowing.
              block_num = block_num + (((LBL & 0x7f) | (((unsigned short)LBH << 4) & 0x80))*256);
              // partition number indicates which 32mb block we access on the CF
              block_num = block_num + (((partition + initPartition) % 4)*65536);
              digitalWrite(statusledPin,HIGH);
              //Serial.print("\r\nRead Block: ");
              //Serial.print(block_num);
              sdstato = sd.readBlock(block_num, (unsigned char*) sector_buffer);    //Reading block from SD Card
              //if (!sdstato) {
              //    Serial.print(F("\r\nRead Err."));
              //}
              encode_data_packet(source);
              //Serial.print(F("\r\nPrepared data packet before Sending\r\n") );
              noInterrupts();
              DDRD=0x40; //set rd as output
              status = SendPacket( (unsigned char*) packet_buffer);
              DDRD=0x00; //set rd back to input so back to tristate
              interrupts();
              //if (status == 1)Serial.print(F("\r\nSent err."));
              digitalWrite(statusledPin,LOW);

              //Serial.print(status);
              //print_packet ((unsigned char*) packet_buffer,packet_length());
              //print_packet ((unsigned char*) sector_buffer,15);
            }
          }
          break;

        case 0x82:  //is a writeblock cmd
          source = packet_buffer[6];
          for (partition=0;partition < NUM_PARTITIONS;partition++) { //Check if its one of ours
            if (device_id[partition] == source) {  //yes it is, then do the write
              // block num 1st byte
              //Added (unsigned short) cast to ensure calculated block is not underflowing.
              block_num = (packet_buffer[19] & 0x7f) | (((unsigned short)packet_buffer[16] << 3) & 0x80);
              // block num second byte
              //Added (unsigned short) cast to ensure calculated block is not underflowing.
              block_num = block_num + (((packet_buffer[20] & 0x7f) | (((unsigned short)packet_buffer[16] << 4) & 0x80))*256);
              //get write data packet, keep trying until no timeout
              noInterrupts();
              DDRC=0xFF;     //set ack to output, sp bus is enabled
              while ((status = ReceivePacket( (unsigned char*) packet_buffer)));
              interrupts();
              //we need to handshake the packet
              PORTC &= ~(_BV(5));   //set ack low
              while(PIND & 0x04);    //wait for req to go low
              // partition number indicates which 32mb block we access on the CF
              block_num = block_num + (((partition + initPartition) % 4)*65536);
              status = decode_data_packet();
              if (status==0) { //ok
                //write block to CF card
                //Serial.print("\r\nWrite Bl. n.r: ");
                //Serial.print(block_num);
                digitalWrite(statusledPin,HIGH);
                sdstato=sd.writeBlock(block_num,(unsigned char*) sector_buffer);      //Write block to SD Card
                if (!sdstato) {
                  //Serial.print(F("\r\nWrite Err."));
                  //Serial.print(F(" Block n.:"));
                  //Serial.print(block_num);
                  status = 6;
                }
              }
              //now return status code to host
              encode_write_status_packet(source,status);
              noInterrupts();
              DDRD=0x40; //set rd as output
              status = SendPacket( (unsigned char*) packet_buffer);
              DDRD=0x00; //set rd back to input so back to tristate
              interrupts();
              //Serial.print(("\r\nSent status Packet Data\r\n") );
              //print_packet ((unsigned char*) sector_buffer,512);
              //print_packet ((unsigned char*) packet_buffer,packet_length());
            }
            digitalWrite(statusledPin,LOW);
          }
          break;

        case 0x83:  //is a format cmd
          source = packet_buffer[6];
          for (partition=0;partition < NUM_PARTITIONS;partition++) { //Check if its one of ours
            if (device_id[partition] == source) {  //yes it is, then reply to the format cmd
              encode_init_reply_packet(source,0x80); //just send back a successful response
              noInterrupts();
              DDRD=0x40; //set rd as output
              status = SendPacket( (unsigned char*) packet_buffer);
              interrupts();
              DDRD=0x00; //set rd back to input so back to tristate
              //Serial.print(("\r\nFormattato!!!\r\n") );
              //print_packet ((unsigned char*) packet_buffer,packet_length());
            }
          }
          break;

        case 0x85:  //is an init cmd
          source = packet_buffer[6];
          if (number_partitions_initialised < NUM_PARTITIONS){  //are all init'd yet
            device_id[number_partitions_initialised - 1] = source; //remember source id for partition
            number_partitions_initialised++;
            status=0x80;           //no, so status=0
          }
          else if (number_partitions_initialised == NUM_PARTITIONS){ // the last one
            device_id[number_partitions_initialised - 1] = source; //remember source id for partition
            number_partitions_initialised++;
            status=0xff;           //yes, so status=non zero
          }

          encode_init_reply_packet(source,status);
          //print_packet ((unsigned char*) packet_buffer,packet_length());

          noInterrupts();
          DDRD=0x40; //set rd as output
          status = SendPacket( (unsigned char*) packet_buffer);
          DDRD=0x00; //set rd back to input so back to tristate
          interrupts();

          //print_packet ((unsigned char*) packet_buffer,packet_length());

          if (number_partitions_initialised - 1 == NUM_PARTITIONS) {
            for (partition=0;partition < 4;partition++) {
              //Serial.print(F("\r\nDrive: "));
              //Serial.print(device_id[partition],HEX);
            }
          }
          break;
      }
    }
  }
}

//*****************************************************************************
// Function: encode_data_packet
// Parameters: source id
// Returns: none
//
// Description: encode 512 byte data packet for read block command from host
// requires the data to be in the sector buffer, and builds the smartport
// packet into the packet buffer
//*****************************************************************************
void encode_data_packet (unsigned char source)
{
  int grpbyte,grpcount;
  unsigned char checksum=0,grpmsb;

  packet_buffer[0] =0xff;   //sync bytes
  packet_buffer[1] =0x3f;
  packet_buffer[2] =0xcf;
  packet_buffer[3] =0xf3;
  packet_buffer[4] =0xfc;
  packet_buffer[5] =0xff;

  packet_buffer[6] =0xc3;   //PBEGIN - start byte
  packet_buffer[7] =0x80;   //DEST - dest id - host
  packet_buffer[8] =source; //SRC - source id - us
  packet_buffer[9] =0x82;   //TYPE - 0x82 = data
  packet_buffer[10]=0x80;   //AUX
  packet_buffer[11]=0x80;   //STAT
  packet_buffer[12]=0x81;   //ODDCNT  - 1 odd byte for 512 byte packet
  packet_buffer[13]=0xC9;   //GRP7CNT - 73 groups of 7 bytes for 512 byte packet

              //total number of packet data bytes for 512 data bytes is 584
              //odd byte
  packet_buffer[14]= ((sector_buffer[0] >> 1) & 0x40) | 0x80;
  packet_buffer[15]= sector_buffer[0] | 0x80;

  //grps of 7
  for (grpcount=0;grpcount<73;grpcount++) //73
  {
    // add group msb byte
    grpmsb=0;
    for (grpbyte=0;grpbyte<7;grpbyte++)
      grpmsb = grpmsb | ((sector_buffer[1+(grpcount*7)+grpbyte] >> (grpbyte+1)) & (0x80 >> (grpbyte+1)));
    packet_buffer[16+(grpcount*8)] = grpmsb | 0x80;   // set msb to one

    // now add the group data bytes bits 6-0
    for (grpbyte=0;grpbyte<7;grpbyte++)
      packet_buffer[17+(grpcount*8)+grpbyte] = sector_buffer[1+(grpcount*7)+grpbyte]| 0x80;
  }

  //add checksum
  for (count=0;count<512;count++)   // xor all the data bytes
    checksum = checksum ^ sector_buffer[count];
  for (count=7;count<14;count++)    // now xor the packet header bytes
    checksum = checksum ^ packet_buffer[count];
  packet_buffer[600] = checksum | 0xaa;      // 1 c6 1 c4 1 c2 1 c0
  packet_buffer[601] = checksum >> 1 | 0xaa; // 1 c7 1 c5 1 c3 1 c1

  //end bytes
  packet_buffer[602] = 0xc8;  //pkt end
  packet_buffer[603] = 0x00;  //mark the end of the packet_buffer

}

//*****************************************************************************
// Function: decode_data_packet
// Parameters: none
// Returns: error code, >0 = error encountered
//
// Description: decode 512 byte data packet for write block command from host
// decodes the data from the packet_buffer into the sector_buffer
//*****************************************************************************
int decode_data_packet (void)
{
  int grpbyte,grpcount;
  unsigned char checksum=0,bit0to6,bit7,oddbits,evenbits;

  //add oddbyte, 1 in a 512 data packet
  sector_buffer[0] = ((packet_buffer[13] << 1) & 0x80) | (packet_buffer[14] & 0x7f);

  // 73 grps of 7 in a 512 byte packet
  for (grpcount=0;grpcount<73;grpcount++)
  {
    for (grpbyte=0;grpbyte<7;grpbyte++) {
      bit7 = (packet_buffer[15+(grpcount*8)] << (grpbyte+1)) & 0x80;
      bit0to6 = (packet_buffer[16+(grpcount*8)+grpbyte]) & 0x7f;
      sector_buffer[1+(grpcount*7)+grpbyte] = bit7 | bit0to6;
    }
  }

  //verify checksum
  for (count=0;count<512;count++)   // xor all the data bytes
    checksum = checksum ^ sector_buffer[count];
  for (count=6;count<13;count++)    // now xor the packet header bytes
    checksum = checksum ^ packet_buffer[count];

  evenbits = packet_buffer[599] & 0x55;
  oddbits = (packet_buffer[600] & 0x55 ) << 1;
  if (checksum == (oddbits | evenbits))
    return 0; //noerror
  else
    return 6; //smartport bus error code

}

//*****************************************************************************
// Function: encode_write_status_packet
// Parameters: source,status
// Returns: none
//
// Description: this is the reply to the write block data packet. The reply
// indicates the status of the write block cmd.
//*****************************************************************************
void encode_write_status_packet(unsigned char source,unsigned char status)
{
  unsigned char checksum=0;

  packet_buffer[0] =0xff;   //sync bytes
  packet_buffer[1] =0x3f;
  packet_buffer[2] =0xcf;
  packet_buffer[3] =0xf3;
  packet_buffer[4] =0xfc;
  packet_buffer[5] =0xff;

  packet_buffer[6] =0xc3;   //PBEGIN - start byte
  packet_buffer[7] =0x80;   //DEST - dest id - host
  packet_buffer[8] =source; //SRC - source id - us
  packet_buffer[9] =0x81;   //TYPE
  packet_buffer[10]=0x80;   //AUX
  packet_buffer[11]=status | 0x80;  //STAT
  packet_buffer[12]=0x80;   //ODDCNT
  packet_buffer[13]=0x80;   //GRP7CNT

  for (count=7;count<14;count++)    // xor the packet header bytes
    checksum = checksum ^ packet_buffer[count];
  packet_buffer[14] = checksum | 0xaa;      // 1 c6 1 c4 1 c2 1 c0
  packet_buffer[15] = checksum >> 1 | 0xaa; // 1 c7 1 c5 1 c3 1 c1

  packet_buffer[16] = 0xc8;  //pkt end
  packet_buffer[17] = 0x00;  //mark the end of the packet_buffer

}

//*****************************************************************************
// Function: encode_init_reply_packet
// Parameters: source
// Returns: none
//
// Description: this is the reply to the init command packet. A reply indicates
// the original dest id has a device on the bus. If the STAT byte is 0, (0x80)
// then this is not the last device in the chain. This is written to support up
// to 4 partions, i.e. devices, so we need to specify when we are doing the last
// init reply.
//*****************************************************************************
void encode_init_reply_packet (unsigned char source, unsigned char status)
{
  unsigned char checksum=0;

  packet_buffer[0] =0xff;   //sync bytes
  packet_buffer[1] =0x3f;
  packet_buffer[2] =0xcf;
  packet_buffer[3] =0xf3;
  packet_buffer[4] =0xfc;
  packet_buffer[5] =0xff;

  packet_buffer[6] =0xc3;   //PBEGIN - start byte
  packet_buffer[7] =0x80;   //DEST - dest id - host
  packet_buffer[8] =source; //SRC - source id - us
  packet_buffer[9] =0x80;   //TYPE
  packet_buffer[10]=0x80;   //AUX
  packet_buffer[11]=status; //STAT - data status

  packet_buffer[12]=0x80;   //ODDCNT   
  packet_buffer[13]=0x80;   //GRP7CNT

  for (count=7;count<14;count++)    // xor the packet header bytes
    checksum = checksum ^ packet_buffer[count];
  packet_buffer[14] = checksum | 0xaa;      // 1 c6 1 c4 1 c2 1 c0
  packet_buffer[15] = checksum >> 1 | 0xaa; // 1 c7 1 c5 1 c3 1 c1

  packet_buffer[16]=0xc8;   //PEND
  packet_buffer[17]=0x00;   //end of packet in buffer   
}

//*****************************************************************************
// Function: encode_status_reply_packet
// Parameters: source
// Returns: none
//
// Description: this is the reply to the status command packet. The reply
// includes following:
// data byte 1
// data byte 2-4 number of blocks. 2 is the LSB and 4 the MSB. Hard coded for 32mb
// partitions. eg 0x00ffff
//*****************************************************************************
void encode_status_reply_packet (unsigned char source)
{
  unsigned char checksum=0;

  packet_buffer[0] =0xff;   //sync bytes
  packet_buffer[1] =0x3f;
  packet_buffer[2] =0xcf;
  packet_buffer[3] =0xf3;
  packet_buffer[4] =0xfc;
  packet_buffer[5] =0xff;

  packet_buffer[6] =0xc3;   //PBEGIN - start byte
  packet_buffer[7] =0x80;   //DEST - dest id - host
  packet_buffer[8] =source; //SRC - source id - us
  packet_buffer[9] =0x81;   //TYPE -status
  packet_buffer[10]=0x80;   //AUX
  packet_buffer[11]=0x80;   //STAT - data status
  packet_buffer[12]=0x84;   //ODDCNT - 4 data bytes
  packet_buffer[13]=0x80;   //GRP7CNT
                //4 odd bytes
  packet_buffer[14]=0xf0;   //odd msb
  packet_buffer[15]=0xf8;   //data 1 -f8
  packet_buffer[16]=0xff;   //data 2 -ff
  packet_buffer[17]=0xff;   //data 3 -ff
  packet_buffer[18]=0x80;   //data 4 -00
                //number of blocks =0x00ffff = 65525 or 32mb
                //calc the data bytes checksum
  checksum = checksum ^ 0xf8;
  checksum = checksum ^ 0xff;
  checksum = checksum ^ 0xff;
  for (count=7;count<14;count++)    // xor the packet header bytes
    checksum = checksum ^ packet_buffer[count];
  packet_buffer[19] = checksum | 0xaa;      // 1 c6 1 c4 1 c2 1 c0
  packet_buffer[20] = checksum >> 1 | 0xaa; // 1 c7 1 c5 1 c3 1 c1

  packet_buffer[21]=0xc8;  //PEND
  packet_buffer[22]=0x00;  //end of packet in buffer
}

//*****************************************************************************
// Function: encode_status_dib_reply_packet
// Parameters: source
// Returns: none
//
// Description: this is the reply to the status command 03 packet. The reply
// includes following:
// data byte 1
// data byte 2-4 number of blocks. 2 is the LSB and 4 the MSB. Hard coded for 32mb
// partitions. eg 0x00ffff
//*****************************************************************************
void encode_status_dib_reply_packet (unsigned char source)
{
  unsigned char checksum=0;

  packet_buffer[0] =0xff;   //sync bytes
  packet_buffer[1] =0x3f;
  packet_buffer[2] =0xcf;
  packet_buffer[3] =0xf3;
  packet_buffer[4] =0xfc;
  packet_buffer[5] =0xff;

  packet_buffer[6] =0xc3;   //PBEGIN - start byte
  packet_buffer[7] =0x80;   //DEST - dest id - host
  packet_buffer[8] =source; //SRC - source id - us
  packet_buffer[9] =0x81;   //TYPE -status
  packet_buffer[10]=0x80;   //AUX
  packet_buffer[11]=0x80;   //STAT - data status
  packet_buffer[12]=0x84;   //ODDCNT - 4 data bytes
  packet_buffer[13]=0x83;   //GRP7CNT - 3 grps of 7
  packet_buffer[14]=0xf0;   //grp1 msb
  packet_buffer[15]=0xf8;   //general status - f8
              //number of blocks =0x00ffff = 65525 or 32mb
  packet_buffer[16]=0xff;   //block size 1 -ff
  packet_buffer[17]=0xff;   //block size 2 -ff
  packet_buffer[18]=0x80;   //block size 3 -00
  packet_buffer[19]=0x8d;   //ID string length - 13 chars
  packet_buffer[20]='S';    //ID string (16 chars total) 
  packet_buffer[21]='m';    //ID string (16 chars total) 
  packet_buffer[22]=0x80;   //grp2 msb
  packet_buffer[23]='a';
  packet_buffer[24]='r';
  packet_buffer[25]='t';
  packet_buffer[26]='p';
  packet_buffer[27]='o';
  packet_buffer[28]='r';
  packet_buffer[29]='t';
  packet_buffer[30]=0x80;   //grp3 msb
  packet_buffer[31]=' ';               
  packet_buffer[32]='C';               
  packet_buffer[33]='F';               
  packet_buffer[34]='A';               
  packet_buffer[35]=' ';               
  packet_buffer[36]=' ';               
  packet_buffer[37]=' ';               
  packet_buffer[38]=0x80;   //odd msb
  packet_buffer[39]=0x82;   //Device type    - 0x02  harddisk
  packet_buffer[40]=0x80;   //Device Subtype - 0x20
  packet_buffer[41]=0x81;   //Firmware version 2 bytes
  packet_buffer[42]=0x90;   //

  //calc the data bytes checksum
  checksum = checksum ^ 0xf8;
  checksum = checksum ^ 0xff;
  checksum = checksum ^ 0xff;
  for (count=7;count<14;count++)    // xor the packet header bytes
    checksum = checksum ^ packet_buffer[count];
  packet_buffer[43] = checksum | 0xaa;      // 1 c6 1 c4 1 c2 1 c0
  packet_buffer[44] = checksum >> 1 | 0xaa; // 1 c7 1 c5 1 c3 1 c1

  packet_buffer[45]=0xc8;  //PEND
  packet_buffer[46]=0x00;  //end of packet in buffer
}

//*****************************************************************************
// Function: verify_cmdpkt_checksum
// Parameters: none
// Returns: 0 = ok, 1 = error
//
// Description: verify the checksum for command packets
//
// &&&&&&&&not used at the moment, no error checking for checksum for cmd packet
//*****************************************************************************
int verify_cmdpkt_checksum(void)
{
  int count=0,length;
  unsigned char evenbits,oddbits,bit7,bit0to6,grpbyte;
  unsigned char calc_checksum=0;  //initial value is 0
  unsigned char pkt_checksum;

  length=packet_length();

  //2 oddbytes in cmd packet
  calc_checksum ^= ((packet_buffer[13] << 1) & 0x80) | (packet_buffer[14] & 0x7f);
  calc_checksum ^= ((packet_buffer[13] << 2) & 0x80) | (packet_buffer[15] & 0x7f);

  // 1 group of 7 in a cmd packet
  for (grpbyte=0;grpbyte<7;grpbyte++) {
    bit7 = (packet_buffer[16] << (grpbyte+1)) & 0x80;
    bit0to6 = (packet_buffer[17+grpbyte]) & 0x7f;
    calc_checksum ^= bit7 | bit0to6;
    }

  // calculate checksum for overhead bytes
  for (count=6;count<13;count++)   // start from first id byte
    calc_checksum ^= packet_buffer[count];

  oddbits = (packet_buffer[length-2] << 1) | 0x01;
  evenbits = packet_buffer[length-3];
  pkt_checksum = oddbits | evenbits;

  //  Serial.print("Pkt Chksum Byte:\r\n");
  //  Serial.print(pkt_checksum,DEC);
  //  Serial.print("Calc Chksum Byte:\r\n");
  //  Serial.print(calc_checksum,DEC);

  if ( pkt_checksum == calc_checksum )
    return 1;
  else
    return 0;

}

//*****************************************************************************
// Function: print_packet
// Parameters: pointer to data, number of bytes to be printed
// Returns: none
//
// Description: prints packet data for debug purposes to the serial port
//*****************************************************************************
/*
 * //Save some RAM commenting out this function and string class
void print_packet (unsigned char* data, int bytes)
{
    int count,row;
    char tbs[4];
    char he2[2];
    char xx;

    //Serial.print(("\r\n"));
    for (count=0;count<bytes;count=count+16){
        sprintf(tbs, "%04X: ", count);
        Serial.print(tbs);
        for (row=0;row<16;row++){
            if (count+row >= bytes)
                Serial.print(F("   "));
            else {
                sprintf(he2, "%02X ", data[count+row]);
                Serial.print(he2);
            }
        }
        Serial.print("-");
        for (row=0;row<16;row++){
            if ((data[count+row] > 31) && (count+row < bytes)&& (data[count+row] <129))
            {
                xx = data[count+row];
                Serial.print(xx);
            }
            else
                Serial.print(("."));
        }
        Serial.print(("\r\n"));
    }
}
*/
//*****************************************************************************
// Function: packet_length
// Parameters: none
// Returns: length
//
// Description: Calculates the length of the packet in the packet_buffer.
// A zero marks the end of the packet data.
//*****************************************************************************
int packet_length (void)
{
  int x=0;

  while (packet_buffer[x++]);
    return x-1;   // point to last packet byte = C8
}

//*****************************************************************************
// Function: init_SD_card
// Parameters: none
// Returns: Error
//
// Description: Init card & print informations about the ATA dispositive and the FAT File System
//*****************************************************************************
int init_SD_card(void)
{
  int i=0;

  Serial.print(F("\r\nCard init"));

  #if USE_SDIO
  if (!sd.begin()) {
  #else  // USE_SDIO
  if (!sd.init(SPI_HALF_SPEED,chipSelect)) {
  #endif

    strcpy_P(chrdata, (char*)pgm_read_word(&(string_table[3]))); //"Card Init Error!"
    u8x8.drawString(0,3,chrdata);
    Serial.print(chrdata);  
    led_err();
    return 1;
  } else {
    return 0;
    //digitalWrite(statusledPin,HIGH);
    //delay(5000);
    //digitalWrite(statusledPin,LOW);
    //delay(1000);                        
  }
}

//*****************************************************************************
// Function: rotate_boot
// Parameters: none
// Returns: none
//
// Description: Cycle by the 4 partition for selecting boot ones, choosing next
// and save it to EEPROM.  Needs REBOOT to get new partition
//*****************************************************************************
int rotate_boot (void)
{
  strcpy_P(chrdata, (char*)pgm_read_word(&(string_table[2])));  //"Eject Pressed!"
  u8x8.drawString(0,3,chrdata);
  Serial.print(chrdata);

  initPartition = initPartition + 1;
  initPartition = initPartition % 4;
  EEPROM.write(0,initPartition);

  strcpy_P(chrdata, (char*)pgm_read_word(&(string_table[1])));  //"Partition:"
  Serial.print(chrdata);
  Serial.print(initPartition,DEC);

  digitalWrite(statusledPin,HIGH);

  while (1);
    // stop programs
}


//*****************************************************************************
// Function: led_err
// Parameters: none
// Returns: nonthing
//
// Description: Flashes status led for show error status
// 
//*****************************************************************************

void led_err(void)
{
  int i=0;
  interrupts();

  strcpy_P(chrdata, (char*)pgm_read_word(&(string_table[4])));  //"Error!"
  u8x8.drawString(0,3,chrdata);

  Serial.print(chrdata);
  pinMode(statusledPin,OUTPUT);

  for (i=0;i<5;i++) { 
    digitalWrite(statusledPin,HIGH);
    delay(1500);
    digitalWrite(statusledPin,LOW);
    delay(100);
    digitalWrite(statusledPin,HIGH);
    delay(1500);
    digitalWrite(statusledPin,HIGH);
  }
}


//*****************************************************************************
// Function: mcuInit
// Parameters: none
// Returns: none
//
// Description: Initialize the ATMega32
//*****************************************************************************
void mcuInit(void)
{
  // Input/Output Ports initialization
  PORTC=0xFF;// Port A initialization
  DDRC=0xFF;

  PORTB=0x00;// Port B initialization
  //  DDRXB=0x00;

  //  PORTXC=0x00;// Port C initialization
  //  DDRXC=0xFF;

  PORTD=0xc0;// Port D initialization
  DDRD=0x00; // leave rd as input, pd6

  // Timer/Counter 0 initialization
  // Clock source: System Clock
  // Clock value: Timer 0 Stopped
  // Mode: Normal top=FFh
  // OC0 output: Disconnected
  //ASSR=0x00;
  //TCCR0=0x00;
  //TCNT0=0x00;
  //OCR0=0x00;

  // Timer/Counter 1 initialization
  // Clock source: System Clock
  // Clock value: Timer 1 Stopped
  // Mode: Normal top=FFFFh
  // OC1A output: Discon.
  // OC1B output: Discon.
  // Noise Canceler: Off
  // Input Capture on Falling Edge
  //TCCR1A=0x00;
  //TCCR1B=0x00;
  //TCNT1H=0x00;
  //TCNT1L=0x00;
  //OCR1AH=0x00;
  //OCR1AL=0x00;
  //OCR1BH=0x00;
  //OCR1BL=0x00;

  // Timer/Counter 2 initialization
  // Clock source: System Clock
  // Clock value: Timer 2 Stopped
  // Mode: Normal top=FFh
  // OC2 output: Disconnected
  //TCCR2=0x00;
  //TCNT2=0x00;
  //OCR2=0x00;


  // INT0: Off
  // INT1: Off
  // INT2: Off
  // INT3: Off
  // INT4: Off
  // INT5: Off
  // INT6: Off
  // INT7: Off
  // EICRA=0x00;
  // EICRB=0x00;
  // EIMSK=0x00;
  // GICR = 0;

  // Timer(s)/Counter(s) Interrupt(s) initialization
  // TIMSK=0x00;
  // ETIMSK=0x00;

  // USART initialization
  // Communication Parameters: 8 Data, 1 Stop, No Parity
  // USART Receiver: Off
  // USART Transmitter: On
  // USART Mode: Asynchronous
  // USART Baud rate: 57600 (double speed = 115200)
  // UCSRA=0x02;
  // UCSRB=0x08;
  // UCSRC=0x06;
  // UBRRH=0x00;
  // UBRRL=0x0e;


  // Analog Comparator initialization
  // Analog Comparator: Off
  // Analog Comparator Input Capture by Timer/Counter 1: Off
  // Analog Comparator Output: Off
  ACSR=0x80;
  //  SFIOR=0x00;
  //noInterrupts();
}
