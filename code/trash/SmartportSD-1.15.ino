
//*****************************************************************************
//
// Apple //c Smartport Compact Flash adapter
// Written by Robert Justice  email: rjustice(at)internode.on.net
// Ported to Arduino UNO with SD Card adapter by Andrea Ottaviani email:
// andrea.ottaviani.69(at)gmail.com
//
// 1.00 - basic read and write working for one partition
// 1.01 - add support for 4 partions, assume no other smartport devices on bus
// 1.02 - add correct handling of device ids, should work with other smartport
// devices
//        before this one in the drive chain.
// 1.03 - fixup smartort bus enable and reset handling, will now start ok with
// llc reset 1.04 - fixup rddata line handling so that it will work with
// internal 5.25 drive. Now if
//        there is a disk in the floppy drive, it will boot first. Otherwise it
//        will boot from the CF
// 1.05 - fix problem with internal disk being always write protected. Code was
// stuck in
//        receivepacket loop, so wrprot(ACK) was always 1. Added timeout support
//        for receivepacket function, so this now times out and goes back to the
//        main loop for another try.
// 1.1  - added support for being connected after unidisk 3.5. Need some more io
// pins to
//        support pass through, this is the next step.
// 1.12 - add eeprom storing of boot partition, cycling by eject button (PA3)
// 1.13 - Fixed an issue where the block number was calculated incorrectly,
// leading to
//        failed operations. Now Total Replay v3 works!
// 1.14 - Added basic OLED support to display info, remove 6 second bootup delay
// left in from testing, code rework. 1.15 - Adjusting ports to work on Arduino
// Pro Micro.
//      - Reworking SPI so display and SD card work on same port for combined
//      TFT & SD Card module.
//
//
// Apple disk interface is connected as follows:
/*                                        | Pro Micro
 * wrprot = pa5 (ack) (output)            | A1 PF6
 * ph0    = pd2 (req) (input)             | D2 PD1
 * ph1    = pd3       (input)             | D3 PD0
 * ph2    = pd4       (input)             | D4 PD4
 * ph3    = pd5       (input)             | D5 PC6
 * rddata = pd6       (output from avr)   | D6 PD7
 * wrdata = pd7       (input to avr)      | D7 PE6
 *
 * _SFR_IO_ADDR(PORTC),5  ACK             | _SFR_IO_ADDR(PORTF),6 
 * _SFR_IO_ADDR(PIND), 2  REQ             | _SFR_IO_ADDR(PIND),1 
 * _SFR_IO_ADDR(PORTD),6  RXD?            | _SFR_IO_ADDR(PORTD),7 
 * _SFR_IO_ADDR(PIND), 7  TXD?            | _SFR_IO_ADDR(PINE),6
 *
 *          TX   1 ╔════╗    RAW
 *          RX   0 ║    ║    GND
 *          GND    ║    ║    RST
 *          GND    ║    ║    VCC
 * PH0      SDA  2 ║    ║ 21 A3     EJECT PIN
 * PH1      SCL  3 ║    ║ 20 A2     STATUS LED
 * PH2      A6   4 ║    ║ 19 A1     WRPROT
 * PH3           5 ║    ║ 18 A0     TFT RST
 * RDATA    A7   6 ║    ║ 15 SCLK   SPICLK
 * WDATA         7 ║    ║ 14 MISO   SPIMISO
 * TFT DC   A8   8 ║    ║ 16 MOSI   SPIMOSI
 * TFT CS   A9   9 ╚════╝ 10 A10    SD CS
 */

//
// led i/o = pa4  (for led on when i/o on boxed version)
// eject button = pa3  (for boxed version, cycle between boot partitions)
//
// Serial port was connected for debug purposes. Most of the prints have been
// commented out. I left these in and these can be uncommented as required for
// debugging. Sometimes the prints add to much delay, so you need to be carefull
// when adding these in.
//
// NOTE: This is uses the ata code from the fat/fat32/ata drivers written by
//       Angelo Bannack and Giordano Bruno Wolaniuk and ported to mega32 by
//       Murray Horn.
//
//*****************************************************************************
//required for vscode, rename this file to .cpp
//#include <Arduino.h>
#include <SD.h>
#include <EEPROM.h>
#include <SPI.h>

#include <string.h>

//ssd1306 by Alexey Dynda
#include <ssd1306.h>

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdio.h>


#define NUM_PARTITIONS 4 // Number of 32MB Prodos partions supported

#define DEFINE 0

#define DEBUGLOG 0



//#define SPI_CLOCK_DIV2 2

void print_hd_info(void);
void encode_data_packet(
    unsigned char source);    // encode smartport 512 byte data packet
int decode_data_packet(void); // decode smartport 512 byte data packet
void encode_write_status_packet(unsigned char source, unsigned char status);
void encode_init_reply_packet(unsigned char source, unsigned char status);
void encode_status_reply_packet(unsigned char source);
void print_packet(unsigned char *data, int bytes);
int packet_length(void);
int init_SD_card(void);
int rotate_boot(void);
void encode_status_dib_reply_packet(unsigned char source);
void mcuInit(void);

void led_err(void);


int partition;

extern "C" unsigned char
ReceivePacket(unsigned char *); // Receive smartport packet assembler function
extern "C" unsigned char
SendPacket(unsigned char *); // send smartport packet assembler function

unsigned char packet_buffer[768]; // smartport packet buffer
unsigned char sector_buffer[512]; // ata sector data buffer
unsigned char status, packet_byte;
int count;
int initPartition;
unsigned char device_id[NUM_PARTITIONS]; // to hold assigned device id's for the
                                         // partitions

//char to store converted numbers etc for serial/OLED display
char chrdata[16];

const uint8_t fontsize = 10 + 2;
const uint8_t line1 = fontsize * 9;
const uint8_t line2 = fontsize * 8;
const uint8_t line3 = fontsize * 7;
const uint8_t line4 = fontsize * 6;
const uint8_t line5 = fontsize * 5;
const uint8_t line6 = fontsize * 4;
const uint8_t line7 = fontsize * 3;
const uint8_t line8 = fontsize * 2;
const uint8_t line9 = fontsize * 1;
const uint8_t line10 = fontsize * 0;

//Define colours printed on the OLED
#define RGB_WHITE 255, 255, 255
#define RGB_RED 255, 0, 0
#define RGB_GREEN 0, 255, 0
#define RGB_BLUE 64, 128, 255

//Define the Apple2c Smartport Pins, make it easier to change references in code.
#define pinPH0        ((PIND & 0x02) >> 1)
#define pinPH1        (PIND & 0x01)
#define pinPH2        ((PIND & 0x10) >> 4)
#define pinPH3        ((PINC & 0x40) >> 6)
#define pinPhases     (((PIND & 0x02) >> 1) + ((PIND & 0x01) << 1) + ((PIND & 0x10) >> 2) + ((PINC & 0x40) >> 3))

#define pinRDATA_INV  PORTD &= ~(_BV(7))

#define pinACK        (PINF & 0x40)
#define pinACK_INV    PORTF &= ~(_BV(6))

#define dirReadIn     DDRD = 0x00
#define dirReadOut    DDRD = (_BV(7))

#define dirAckIn      DDRF &= ~(_BV(6))
#define dirAckOut     DDRF |= _BV(6)


// SPI Pins
// ** MOSI - pin 16 Arduino Pro Micro
// ** MISO - pin 14 Arduino Pro Micro
// ** CLK  - pin 15 Arduino Pro Micro
const uint8_t SD_CS = 10;

const uint8_t TFT_CS = 9;
const uint8_t TFT_DC = 8;
const uint8_t TFT_RST = A0;

// Misc drive I/O
const uint8_t pinEject = A3;
const uint8_t pinStatusLED = A2;

// Initialize at highest supported speed not over 50 MHz.
// Reduce max speed if errors occur.

/*
 * Set DISABLE_CHIP_SELECT to disable a second SPI device.
 * For example, with the Ethernet shield, set DISABLE_CHIP_SELECT
 * to 10 to disable the Ethernet controller.
 */
const int8_t DISABLE_CHIP_SELECT = 0; // -1

//Define SD Card
Sd2Card sd;

//Define OLED


//*****************************************************************************
// Function: Setup
// Parameters: none
// Returns: none
//
// Description: Start up function for the Arduino.
//*****************************************************************************

void setup() {
  // put your setup code here, to run once:
  mcuInit();

  //Power on drive LED at start of setup
  digitalWrite(pinStatusLED, HIGH);

  // Init serial port
#ifdef DEBUGLOG
  Serial.begin(9600);
  while (!Serial) {
    //Wait for serial port to initalise, esp for USB one in Pro Micro
    ;
  }
#endif

  // Init screen
/*
  ucg.powerDown();
    ucg.powerUp();

  ucg.begin(UCG_FONT_MODE_SOLID);
  ucg.clearScreen();
  ucg.setFont(ucg_font_8x13_mf);
  ucg.setPrintDir(1);

  ucg.setColor(RGB_WHITE);
  ucg.setPrintPos(line1, 0);
  ucg.print(F("SmartportSD v1.15"));
*/

    ssd1306_setFixedFont(ssd1306xled_font6x8);
    st7735_128x160_spi_init(TFT_RST, TFT_CS, TFT_DC);

    // RGB functions do not work in default SSD1306 compatible mode
    ssd1306_setMode( LCD_MODE_NORMAL );
    ssd1306_clearScreen8( );

    ssd1306_setColor(RGB_COLOR8(255,255,0));
    ssd1306_printFixed8(0,  8, "Normal text", STYLE_NORMAL);
    ssd1306_setColor(RGB_COLOR8(0,255,0));
    ssd1306_printFixed8(0, 16, "bold text?", STYLE_BOLD);
    ssd1306_setColor(RGB_COLOR8(0,255,255));
    ssd1306_printFixed8(0, 24, "Italic text?", STYLE_ITALIC);
    ssd1306_negativeMode();
    ssd1306_setColor(RGB_COLOR8(255,255,255));
    ssd1306_printFixed8(0, 32, "Inverted bold?", STYLE_BOLD);
    ssd1306_positiveMode();

#ifdef DEBUGLOG
  Serial.println(F("#####################"));
  Serial.println(F("# SmartportSD v1.15 #"));
  Serial.println(F("#####################"));
#endif

  //Read the boot partition number from EEPROM
  initPartition = EEPROM.read(0);

  if (initPartition == 0xFF)
    initPartition = 0;
  initPartition = (initPartition % 4);

/*
  ucg.setPrintPos(line2, 0);
  ucg.print(F("Boot Partition:"));
  itoa(initPartition, chrdata, 16);
  ucg.setColor(RGB_BLUE);
  ucg.print(chrdata);
  ucg.powerDown();
*/

#ifdef DEBUGLOG
  Serial.print(F("Boot partition: "));
  Serial.println(initPartition, DEC);
#endif

  if (init_SD_card())
    while (1)
    ;

  //Turn off drive LED at end of init/startup  
  digitalWrite(pinStatusLED, LOW);

}

//*****************************************************************************
// Function: main loop
// Parameters: none
// Returns: 0
//
// Description: Main function for Apple //c Smartport SD
//*****************************************************************************

void loop() {
  // put your main code here, to run repeatedly:

  unsigned long int block_num;
  unsigned char LBH, LBL, LBN;

  int number_partitions_initialised = 1;
  int noid = 0;
  //int count;
  uint8_t sdstato;
  unsigned char source, status, status_code;
#ifdef DEBUGLOG
  Serial.println(F("loop"));
#endif

/*
    //SD Storage test
    //################################

    csd_t csd;
    Serial.print(F("CSD: 0x"));
    Serial.println(sd.readCSD(&csd),HEX);
    Serial.print(F("Error Code: "));    Serial.print(sd.errorCode());    Serial.print(F(", "));    Serial.println(sd.errorData());

    cid_t cid;
    Serial.print(F("CID: 0x"));
    Serial.println(sd.readCID(&cid),HEX);
    Serial.print(F("Error Code: "));    Serial.print(sd.errorCode());    Serial.print(F(", "));    Serial.println(sd.errorData());

    Serial.print(F("Card Size:"));
    Serial.println(sd.cardSize());
    Serial.print(F("Error Code: "));    Serial.print(sd.errorCode());    Serial.print(F(", "));    Serial.println(sd.errorData());


    Serial.println(F("Printing block 0."));
    block_num = 0;
    sdstato = sd.readBlock(block_num, (unsigned char *)sector_buffer);
    Serial.print(F("Result:"));
    Serial.println(sdstato);
    Serial.print(F("Error Code:"));    Serial.print(sd.errorCode());    Serial.print(F(", "));    Serial.println(sd.errorData());
    print_packet ((unsigned char*) sector_buffer,512);
    
    //################################
*/


  dirReadIn;

  pinRDATA_INV; // set RD low
  interrupts();
  while (1) {

    if (digitalRead(pinEject) == LOW)
      rotate_boot();

    noid = 0;    // reset noid flag
    dirAckIn; // set ack (wrprot) to input to avoid clashing with other
                 // devices when sp bus is not enabled

    // read phase lines to check for smartport reset or enable
    //phases = (PIND & 0x3c) >> 2;

    // Serial.print(pinPhases);
    switch pinPhases 
    {
      
    // phase lines for smartport bus reset
    // ph3=0 ph2=1 ph1=0 ph0=1
    case 0x05:
#ifdef DEBUGLOG
      Serial.println(F("Reset"));
#endif
      // Serial.print(number_partitions_initialised);
      // monitor phase lines for reset to clear
      while (pinPhases == 0x05)
        ;

      number_partitions_initialised = 1; // reset number of partitions init'd
      noid = 0;                          // to check if needed
      for (partition = 0; partition < NUM_PARTITIONS;
           partition++) // clear device_id table
        device_id[partition] = 0;
      break;

    // phase lines for smartport bus enable
    // ph3=1 ph2=x ph1=1 ph0=x
    case 0x0a:
    case 0x0b:
    case 0x0e:
    case 0x0f:
#ifdef DEBUGLOG    
      Serial.println("E"); //this is timing sensitive, so can't print to
#endif
      // much here as it takes to long
      noInterrupts();
      dirAckOut; // set ack to output, sp bus is enabled
      if ((status = ReceivePacket((unsigned char *)packet_buffer))) {
        interrupts();
#ifdef DEBUGLOG    
        Serial.print("~");
#endif
        break; // error timeout, break and loop again
      }
      interrupts();

      // lets check if the pkt is for us
      if (packet_buffer[14] != 0x85) // if its an init pkt, then assume its for us and continue on
      {
        // else check if its our one of our id's
        for (partition = 0; partition < NUM_PARTITIONS; partition++) {
          if (device_id[partition] != packet_buffer[6]) // destination id
            noid++;
        }
        if (noid == NUM_PARTITIONS) // not one of our id's
        {
#ifdef DEBUGLOG
          Serial.println("not ours");
#endif
          dirAckIn;        // set ack to input, so lets not interfere
          pinACK_INV; // set ack low, for next time its an output
          while pinACK
            ; // wait till low other dev has finished receiving it
#ifdef DEBUGLOG
          Serial.println("a");
          print_packet ((unsigned char*) packet_buffer,packet_length());
#endif

          // assume its a cmd packet, cmd code is in byte 14
          // now we need to work out what type of packet and stay out of the way
          switch (packet_buffer[14]) {
          case 0x80: // is a status cmd
          case 0x83: // is a format cmd
          case 0x81: // is a readblock cmd
            while (!pinACK)
              ; // wait till high
#ifdef DEBUGLOG
            Serial.print("A");
#endif
            while pinACK
              ; // wait till low
#ifdef DEBUGLOG
            Serial.print("a");
#endif
            while (!pinACK)
              ; // wait till high
#ifdef DEBUGLOG
            Serial.println("A.");
#endif
            break;
          case 0x82: // is a writeblock cmd
            while (!pinACK)
              ; // wait till high
#ifdef DEBUGLOG
            Serial.print("W");
#endif
            while pinACK
              ; // wait till low
#ifdef DEBUGLOG
            Serial.print("w");
#endif
            while (!pinACK)
              ; // wait till high
#ifdef DEBUGLOG
            Serial.println("W.");
#endif
            while pinACK
              ; // wait till low
#ifdef DEBUGLOG
            Serial.print("w");
#endif
            while (!pinACK)
              ; // wait till high
#ifdef DEBUGLOG
            Serial.println("W.");
#endif
            break;
          }
          break; // not one of ours
        }
      }

      // else it is ours, we need to handshake the packet
      pinACK_INV; // set ack low
      while (pinPH0)
        ; // wait for req to go low
      // assume its a cmd packet, cmd code is in byte 14
#ifdef DEBUGLOG
      Serial.print("cmd 0x");
      Serial.println(packet_buffer[14],HEX);
      //print_packet ((unsigned char*) packet_buffer,packet_length());
#endif

      switch (packet_buffer[14])
      {
/* 
 * Status Command
 * 
 */
      case 0x80: // is a status cmd
        digitalWrite(pinStatusLED, HIGH);
        source = packet_buffer[6];
        for (partition = 0; partition < NUM_PARTITIONS; partition++) {            // Check if its one of ours
          if (device_id[partition] == source) {                                   // yes it is, then reply
                                                                                  // Added (unsigned short) cast to ensure calculated
                                                                                  //   block number is not underflowing.
            status_code = (packet_buffer[17] & 0x7f) | (((unsigned short)packet_buffer[16] << 3) & 0x80);
            if (status_code == 0x03) {                                            // if statcode=3, then status with device info block
              encode_status_dib_reply_packet(source);
            } else {                                                              // else just return device status
              encode_status_reply_packet(source);
            }
            noInterrupts();
            dirReadOut;                                                           // set rd as output
            status = SendPacket((unsigned char *)packet_buffer);
            dirReadIn;                                                            // set rd back to input so back to tristate
            interrupts();
            
#ifdef DEBUGLOG
           // Serial.println(F("Status Command"));
            Serial.print(F("sts:"));
            Serial.println(status,HEX);
            if (status == 1)
              Serial.println(F("Packet send error."));

            //print_packet ((unsigned char*) packet_buffer,packet_length());
            digitalWrite(pinStatusLED, LOW);
#endif
          }
        }
        break;

/* 
 * Read Block Command
 * 
 */
      case 0x81: // is a readblock cmd
        source = packet_buffer[6];
        LBH = packet_buffer[16];
        LBL = packet_buffer[20];
        LBN = packet_buffer[19];
        for (partition = 0; partition < NUM_PARTITIONS; partition++) {                     // Check if its one of ours
          if (device_id[partition] == source) {                                            // yes it is, then do the read
                                                                                           // block num 1st byte
                                                                                           // Added (unsigned short) cast to ensure calculated block is not underflowing.
            block_num = (LBN & 0x7f) | (((unsigned short)LBH << 3) & 0x80);
                                                                                           // block num second byte
                                                                                           // Added (unsigned short) cast to ensure calculated block is not underflowing.
            block_num = block_num + (((LBL & 0x7f) | (((unsigned short)LBH << 4) & 0x80)) * 256);
                                                                                           // partition number indicates which 32mb block we access on the CF
            block_num = block_num + (((partition + initPartition) % 4) * 65536);
            digitalWrite(pinStatusLED, HIGH);
#ifdef DEBUGLOG
            Serial.println(F("Readblock Command"));
            Serial.print(F("Drive: 0x"));
            Serial.println(source,HEX);

            Serial.print(F("Block: 0x"));
            Serial.println(block_num,HEX);
 #endif
           sdstato = sd.readBlock(block_num, (unsigned char *)sector_buffer);               // Reading block from SD Card
#ifdef DEBUGLOG
            if (!sdstato) {
              Serial.print(F("SD Error Code: 0x"));
              Serial.println(sd.errorCode(),HEX);
              Serial.print(F("SD Error Data: 0x"));
              Serial.println(sd.errorData(),HEX);
             
            }
#endif
            
            encode_data_packet(source);

            noInterrupts();
            dirReadOut;                                                                     // set rd as output
            status = SendPacket((unsigned char *)packet_buffer);
            dirReadIn;                                                                      // set rd back to input so back to tristate
            interrupts();

            digitalWrite(pinStatusLED, LOW);
#ifdef DEBUGLOG
            Serial.print(F("sts:"));
            Serial.println(status,HEX);
            if (status == 1)
              Serial.println(F("Packet send error."));
            //print_packet ((unsigned char*) packet_buffer,packet_length());
            //print_packet ((unsigned char*) sector_buffer,15);
#endif
          }
        }
        break;

/* 
 * Write Block Command
 * 
 */
      case 0x82: // is a writeblock cmd
        source = packet_buffer[6];
        for (partition = 0; partition < NUM_PARTITIONS; partition++) {                      // Check if its one of ours
          if (device_id[partition] == source) {                                             // yes it is, then do the write
            // block num 1st byte
            // Added (unsigned short) cast to ensure calculated block is not underflowing.
            block_num = (packet_buffer[19] & 0x7f) | (((unsigned short)packet_buffer[16] << 3) & 0x80);
            // block num second byte
            // Added (unsigned short) cast to ensure calculated block is not
            // underflowing.
            block_num = block_num + (((packet_buffer[20] & 0x7f) | (((unsigned short)packet_buffer[16] << 4) & 0x80)) * 256);
            // get write data packet, keep trying until no timeout
            noInterrupts();
            dirAckOut;                                                                      // set ack to output, sp bus is enabled
            while ((status = ReceivePacket((unsigned char *)packet_buffer)))
              ;
            interrupts();
                                                                                            // we need to handshake the packet
            pinACK_INV;                                                                     // set ack low
            while (pinPH0)
              ;                                                                             // wait for req to go low
                                                                                            // partition number indicates which 32mb block we access on the CF
            block_num = block_num + (((partition + initPartition) % 4) * 65536);
            status = decode_data_packet();
            if (status == 0) {                                                              // ok
                                                                                            // write block to CF card
              digitalWrite(pinStatusLED, HIGH);
#ifdef DEBUGLOG
            Serial.println(F("Writeblock Command"));
            Serial.print(F("Drive: 0x"));
            Serial.println(source,HEX);

            Serial.print(F("Block: 0x"));
            Serial.println(block_num,HEX);
 #endif
              sdstato = sd.writeBlock(block_num, (unsigned char *)sector_buffer);           // Write block to SD Card
            if (!sdstato) {
#ifdef DEBUGLOG
              Serial.println(F("SD Write failed!"));
#endif
                status = 6;
              }
            }
                                                                                            // now return status code to host
            encode_write_status_packet(source, status);
            noInterrupts();
            dirReadOut;                                                                     // set rd as output
            status = SendPacket((unsigned char *)packet_buffer);
            dirReadIn;                                                                      // set rd back to input so back to tristate
            interrupts();
#ifdef DEBUGLOG
            Serial.print(F("sts:"));
            Serial.println(status,HEX);
            if (status == 1)
              Serial.println(F("Packet send error."));

            //print_packet ((unsigned char*) packet_buffer,packet_length());
            //print_packet ((unsigned char*) sector_buffer,15);
#endif
          }
          digitalWrite(pinStatusLED, LOW);
        }
        break;

/* 
 * Format Command
 * 
 */
      case 0x83: // is a format cmd
        source = packet_buffer[6];
        for (partition = 0; partition < NUM_PARTITIONS; partition++) {            // Check if its one of ours
          if (device_id[partition] == source) {                                   // yes it is, then reply to the format cmd
            encode_init_reply_packet(source, 0x80);                               // just send back a successful response
            noInterrupts();
            dirReadOut;                                                           // set rd as output
            status = SendPacket((unsigned char *)packet_buffer);
            dirReadIn;                                                            // set rd back to input so back to tristate
            interrupts();
#ifdef DEBUGLOG
            Serial.println(F("Formatting!"));
            Serial.print(F("sts:"));
            Serial.println(status,HEX);
//            print_packet ((unsigned char*) packet_buffer,packet_length());
#endif
          }
        }
        break;

/* 
 * Init Command
 * 
 */
      case 0x85: // is an init cmd

#ifdef DEBUGLOG
//        Serial.println(F("Init CMD"));
#endif

        source = packet_buffer[6];
        if (number_partitions_initialised < NUM_PARTITIONS) { // are all init'd yet
          device_id[number_partitions_initialised - 1] = source; // remember source id for partition
          number_partitions_initialised++;
          status = 0x80; // no, so status=0
        } else if (number_partitions_initialised >= NUM_PARTITIONS) { // the last one
          device_id[number_partitions_initialised - 1] = source; // remember source id for partition
          number_partitions_initialised++;
          status = 0xff; // yes, so status=non zero
        }

#ifdef DEBUGLOG
//        Serial.print(F("number_partitions_initialised: 0x"));
//        Serial.println(number_partitions_initialised,HEX);

        //if (number_partitions_initialised - 1 == NUM_PARTITIONS) {
//          for (partition = 0; partition < 4; partition++) {
//            Serial.print(F("Drive: "));
//            Serial.println(device_id[partition],HEX);
//          }
        //}
#endif
        
        encode_init_reply_packet(source, status);
#ifdef DEBUGLOG
//        Serial.print(F("Status: 0x"));
//        Serial.println(status,HEX);
//        Serial.println(F("Reply:"));
//        print_packet ((unsigned char*) packet_buffer,packet_length());
#endif

        noInterrupts();
        dirReadOut; // set rd as output
        status = SendPacket((unsigned char *)packet_buffer);
        dirReadIn; // set rd back to input so back to tristate
        interrupts();

#ifdef DEBUGLOG
        Serial.print(F("sts:"));
        Serial.println(status);
        if (number_partitions_initialised - 1 == NUM_PARTITIONS) {
          Serial.print(F("Drives:"));
          Serial.println(number_partitions_initialised-1);
          for (partition = 0; partition < 4; partition++) {
            Serial.print(F("Drv:"));
            Serial.println(device_id[partition],HEX);
          }
        }
#endif
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
void encode_data_packet(unsigned char source) {
  int grpbyte, grpcount;
  unsigned char checksum = 0, grpmsb;

  packet_buffer[0] = 0xff; // sync bytes
  packet_buffer[1] = 0x3f;
  packet_buffer[2] = 0xcf;
  packet_buffer[3] = 0xf3;
  packet_buffer[4] = 0xfc;
  packet_buffer[5] = 0xff;

  packet_buffer[6] = 0xc3;   // PBEGIN - start byte
  packet_buffer[7] = 0x80;   // DEST - dest id - host
  packet_buffer[8] = source; // SRC - source id - us
  packet_buffer[9] = 0x82;   // TYPE - 0x82 = data
  packet_buffer[10] = 0x80;  // AUX
  packet_buffer[11] = 0x80;  // STAT
  packet_buffer[12] = 0x81;  // ODDCNT  - 1 odd byte for 512 byte packet
  packet_buffer[13] = 0xC9;  // GRP7CNT - 73 groups of 7 bytes for 512 byte
                            // packet

  // total number of packet data bytes for 512 data bytes is 584
  // odd byte
  packet_buffer[14] = ((sector_buffer[0] >> 1) & 0x40) | 0x80;
  packet_buffer[15] = sector_buffer[0] | 0x80;

  // grps of 7
  for (grpcount = 0; grpcount < 73; grpcount++) // 73
  {
    // add group msb byte
    grpmsb = 0;
    for (grpbyte = 0; grpbyte < 7; grpbyte++)
      grpmsb = grpmsb | ((sector_buffer[1 + (grpcount * 7) + grpbyte] >> (grpbyte + 1)) & (0x80 >> (grpbyte + 1)));
    packet_buffer[16 + (grpcount * 8)] = grpmsb | 0x80; // set msb to one

    // now add the group data bytes bits 6-0
    for (grpbyte = 0; grpbyte < 7; grpbyte++)
      packet_buffer[17 + (grpcount * 8) + grpbyte] = sector_buffer[1 + (grpcount * 7) + grpbyte] | 0x80;
  }

  // add checksum
  for (count = 0; count < 512; count++) // xor all the data bytes
    checksum = checksum ^ sector_buffer[count];
  for (count = 7; count < 14; count++) // now xor the packet header bytes
    checksum = checksum ^ packet_buffer[count];
  packet_buffer[600] = checksum | 0xaa;      // 1 c6 1 c4 1 c2 1 c0
  packet_buffer[601] = checksum >> 1 | 0xaa; // 1 c7 1 c5 1 c3 1 c1

  // end bytes
  packet_buffer[602] = 0xc8; // pkt end
  packet_buffer[603] = 0x00; // mark the end of the packet_buffer
}

//*****************************************************************************
// Function: decode_data_packet
// Parameters: none
// Returns: error code, >0 = error encountered
//
// Description: decode 512 byte data packet for write block command from host
// decodes the data from the packet_buffer into the sector_buffer
//*****************************************************************************
int decode_data_packet(void) {
  int grpbyte, grpcount;
  unsigned char checksum = 0, bit0to6, bit7, oddbits, evenbits;

  // add oddbyte, 1 in a 512 data packet
  sector_buffer[0] = ((packet_buffer[13] << 1) & 0x80) | (packet_buffer[14] & 0x7f);

  // 73 grps of 7 in a 512 byte packet
  for (grpcount = 0; grpcount < 73; grpcount++) {
    for (grpbyte = 0; grpbyte < 7; grpbyte++) {
      bit7 = (packet_buffer[15 + (grpcount * 8)] << (grpbyte + 1)) & 0x80;
      bit0to6 = (packet_buffer[16 + (grpcount * 8) + grpbyte]) & 0x7f;
      sector_buffer[1 + (grpcount * 7) + grpbyte] = bit7 | bit0to6;
    }
  }

  // verify checksum
  for (count = 0; count < 512; count++) // xor all the data bytes
    checksum = checksum ^ sector_buffer[count];
  for (count = 6; count < 13; count++) // now xor the packet header bytes
    checksum = checksum ^ packet_buffer[count];

  evenbits = packet_buffer[599] & 0x55;
  oddbits = (packet_buffer[600] & 0x55) << 1;
  if (checksum == (oddbits | evenbits))
    return 0; // noerror
  else
    return 6; // smartport bus error code
}

//*****************************************************************************
// Function: encode_write_status_packet
// Parameters: source,status
// Returns: none
//
// Description: this is the reply to the write block data packet. The reply
// indicates the status of the write block cmd.
//*****************************************************************************
void encode_write_status_packet(unsigned char source, unsigned char status) {
  unsigned char checksum = 0;

  packet_buffer[0] = 0xff; // sync bytes
  packet_buffer[1] = 0x3f;
  packet_buffer[2] = 0xcf;
  packet_buffer[3] = 0xf3;
  packet_buffer[4] = 0xfc;
  packet_buffer[5] = 0xff;

  packet_buffer[6] = 0xc3;           // PBEGIN - start byte
  packet_buffer[7] = 0x80;           // DEST - dest id - host
  packet_buffer[8] = source;         // SRC - source id - us
  packet_buffer[9] = 0x81;           // TYPE
  packet_buffer[10] = 0x80;          // AUX
  packet_buffer[11] = status | 0x80; // STAT
  packet_buffer[12] = 0x80;          // ODDCNT
  packet_buffer[13] = 0x80;          // GRP7CNT

  for (count = 7; count < 14; count++) // xor the packet header bytes
    checksum = checksum ^ packet_buffer[count];
  packet_buffer[14] = checksum | 0xaa;      // 1 c6 1 c4 1 c2 1 c0
  packet_buffer[15] = checksum >> 1 | 0xaa; // 1 c7 1 c5 1 c3 1 c1

  packet_buffer[16] = 0xc8; // pkt end
  packet_buffer[17] = 0x00; // mark the end of the packet_buffer
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
void encode_init_reply_packet(unsigned char source, unsigned char status) {
  unsigned char checksum = 0;

  packet_buffer[0] = 0xff; // sync bytes
  packet_buffer[1] = 0x3f;
  packet_buffer[2] = 0xcf;
  packet_buffer[3] = 0xf3;
  packet_buffer[4] = 0xfc;
  packet_buffer[5] = 0xff;

  packet_buffer[6] = 0xc3;    // PBEGIN - start byte
  packet_buffer[7] = 0x80;    // DEST - dest id - host
  packet_buffer[8] = source;  // SRC - source id - us
  packet_buffer[9] = 0x80;    // TYPE
  packet_buffer[10] = 0x80;   // AUX
  packet_buffer[11] = status; // STAT - data status

  packet_buffer[12] = 0x80; // ODDCNT
  packet_buffer[13] = 0x80; // GRP7CNT

  for (count = 7; count < 14; count++) // xor the packet header bytes
    checksum = checksum ^ packet_buffer[count];
  packet_buffer[14] = checksum | 0xaa;      // 1 c6 1 c4 1 c2 1 c0
  packet_buffer[15] = checksum >> 1 | 0xaa; // 1 c7 1 c5 1 c3 1 c1

  packet_buffer[16] = 0xc8; // PEND
  packet_buffer[17] = 0x00; // end of packet in buffer
}

//*****************************************************************************
// Function: encode_status_reply_packet
// Parameters: source
// Returns: none
//
// Description: this is the reply to the status command packet. The reply
// includes following:
// data byte 1
// data byte 2-4 number of blocks. 2 is the LSB and 4 the MSB. Hard coded for
// 32mb partitions. eg 0x00ffff
//*****************************************************************************
void encode_status_reply_packet(unsigned char source) {
  unsigned char checksum = 0;

  packet_buffer[0] = 0xff; // sync bytes
  packet_buffer[1] = 0x3f;
  packet_buffer[2] = 0xcf;
  packet_buffer[3] = 0xf3;
  packet_buffer[4] = 0xfc;
  packet_buffer[5] = 0xff;

  packet_buffer[6] = 0xc3;   // PBEGIN - start byte
  packet_buffer[7] = 0x80;   // DEST - dest id - host
  packet_buffer[8] = source; // SRC - source id - us
  packet_buffer[9] = 0x81;   // TYPE -status
  packet_buffer[10] = 0x80;  // AUX
  packet_buffer[11] = 0x80;  // STAT - data status
  packet_buffer[12] = 0x84;  // ODDCNT - 4 data bytes
  packet_buffer[13] = 0x80;  // GRP7CNT
                            // 4 odd bytes
  packet_buffer[14] = 0xf0; // odd msb
  packet_buffer[15] = 0xf8; // data 1 -f8
  packet_buffer[16] = 0xff; // data 2 -ff
  packet_buffer[17] = 0xff; // data 3 -ff
  packet_buffer[18] = 0x80; // data 4 -00
                            // number of blocks =0x00ffff = 65525 or 32mb
                            // calc the data bytes checksum
  checksum = checksum ^ 0xf8;
  checksum = checksum ^ 0xff;
  checksum = checksum ^ 0xff;
  for (count = 7; count < 14; count++) // xor the packet header bytes
    checksum = checksum ^ packet_buffer[count];
  packet_buffer[19] = checksum | 0xaa;      // 1 c6 1 c4 1 c2 1 c0
  packet_buffer[20] = checksum >> 1 | 0xaa; // 1 c7 1 c5 1 c3 1 c1

  packet_buffer[21] = 0xc8; // PEND
  packet_buffer[22] = 0x00; // end of packet in buffer
}

//*****************************************************************************
// Function: encode_status_dib_reply_packet
// Parameters: source
// Returns: none
//
// Description: this is the reply to the status command 03 packet. The reply
// includes following:
// data byte 1
// data byte 2-4 number of blocks. 2 is the LSB and 4 the MSB. Hard coded for
// 32mb partitions. eg 0x00ffff
//*****************************************************************************
void encode_status_dib_reply_packet(unsigned char source) {
  unsigned char checksum = 0;

  packet_buffer[0] = 0xff; // sync bytes
  packet_buffer[1] = 0x3f;
  packet_buffer[2] = 0xcf;
  packet_buffer[3] = 0xf3;
  packet_buffer[4] = 0xfc;
  packet_buffer[5] = 0xff;

  packet_buffer[6] = 0xc3;   // PBEGIN - start byte
  packet_buffer[7] = 0x80;   // DEST - dest id - host
  packet_buffer[8] = source; // SRC - source id - us
  packet_buffer[9] = 0x81;   // TYPE -status
  packet_buffer[10] = 0x80;  // AUX
  packet_buffer[11] = 0x80;  // STAT - data status
  packet_buffer[12] = 0x84;  // ODDCNT - 4 data bytes
  packet_buffer[13] = 0x83;  // GRP7CNT - 3 grps of 7
  packet_buffer[14] = 0xf0;  // grp1 msb
  packet_buffer[15] = 0xf8;  // general status - f8
                            // number of blocks =0x00ffff = 65525 or 32mb
  packet_buffer[16] = 0xff; // block size 1 -ff
  packet_buffer[17] = 0xff; // block size 2 -ff
  packet_buffer[18] = 0x80; // block size 3 -00
  packet_buffer[19] = 0x8d; // ID string length - 13 chars
  packet_buffer[20] = 'S';  // ID string (16 chars total)
  packet_buffer[21] = 'm';  // ID string (16 chars total)
  packet_buffer[22] = 0x80; // grp2 msb
  packet_buffer[23] = 'a';
  packet_buffer[24] = 'r';
  packet_buffer[25] = 't';
  packet_buffer[26] = 'p';
  packet_buffer[27] = 'o';
  packet_buffer[28] = 'r';
  packet_buffer[29] = 't';
  packet_buffer[30] = 0x80; // grp3 msb
  packet_buffer[31] = ' ';
  packet_buffer[32] = 'S';
  packet_buffer[33] = 'D';
  packet_buffer[34] = ' ';
  packet_buffer[35] = ' ';
  packet_buffer[36] = ' ';
  packet_buffer[37] = ' ';
  packet_buffer[38] = 0x80; // odd msb
  packet_buffer[39] = 0x82; // Device type    - 0x02  harddisk
  packet_buffer[40] = 0x80; // Device Subtype - 0x20
  packet_buffer[41] = 0x81; // Firmware version 2 bytes
  packet_buffer[42] = 0x90; //

  // calc the data bytes checksum
  checksum = checksum ^ 0xf8;
  checksum = checksum ^ 0xff;
  checksum = checksum ^ 0xff;
  for (count = 7; count < 14; count++) // xor the packet header bytes
    checksum = checksum ^ packet_buffer[count];
  packet_buffer[43] = checksum | 0xaa;      // 1 c6 1 c4 1 c2 1 c0
  packet_buffer[44] = checksum >> 1 | 0xaa; // 1 c7 1 c5 1 c3 1 c1

  packet_buffer[45] = 0xc8; // PEND
  packet_buffer[46] = 0x00; // end of packet in buffer
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
int verify_cmdpkt_checksum(void) {
  int count = 0, length;
  unsigned char evenbits, oddbits, bit7, bit0to6, grpbyte;
  unsigned char calc_checksum = 0; // initial value is 0
  unsigned char pkt_checksum;

  length = packet_length();

  // 2 oddbytes in cmd packet
  calc_checksum ^= ((packet_buffer[13] << 1) & 0x80) | (packet_buffer[14] & 0x7f);
  calc_checksum ^= ((packet_buffer[13] << 2) & 0x80) | (packet_buffer[15] & 0x7f);

  // 1 group of 7 in a cmd packet
  for (grpbyte = 0; grpbyte < 7; grpbyte++) {
    bit7 = (packet_buffer[16] << (grpbyte + 1)) & 0x80;
    bit0to6 = (packet_buffer[17 + grpbyte]) & 0x7f;
    calc_checksum ^= bit7 | bit0to6;
  }

  // calculate checksum for overhead bytes
  for (count = 6; count < 13; count++) // start from first id byte
    calc_checksum ^= packet_buffer[count];

  oddbits = (packet_buffer[length - 2] << 1) | 0x01;
  evenbits = packet_buffer[length - 3];
  pkt_checksum = oddbits | evenbits;

#ifdef DEBUGLOG
  Serial.print(F("Pkt Chksum Byte:\r\n"));
  Serial.println(pkt_checksum,DEC);
  Serial.print(F("Calc Chksum Byte:\r\n"));
  Serial.println(calc_checksum,DEC);
#endif

  if (pkt_checksum == calc_checksum)
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
            if ((data[count+row] > 31) && (count+row < bytes) && (data[count+row]<129))
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

//*****************************************************************************
// Function: packet_length
// Parameters: none
// Returns: length
//
// Description: Calculates the length of the packet in the packet_buffer.
// A zero marks the end of the packet data.
//*****************************************************************************
int packet_length(void) {
  int x = 0;

  while (packet_buffer[x++])
    ;
  return x - 1; // point to last packet byte = C8
}

//*****************************************************************************
// Function: rotate_boot
// Parameters: none
// Returns: none
//
// Description: Cycle by the 4 partition for selecting boot ones, choosing next
// and save it to EEPROM.  Needs REBOOT to get new partition
//*****************************************************************************
int rotate_boot(void) {

#ifdef DEBUGLOG
  Serial.println(F("Eject Pressed!"));
#endif
  //ucg.setColor(RGB_RED);
  //ucg.setPrintPos(line3, 0);
  //ucg.print(F("Eject Pressed!"));

  initPartition = initPartition + 1;
  initPartition = initPartition % 4;
  EEPROM.write(0, initPartition);

  //ucg.setColor(RGB_WHITE);
  //ucg.setPrintPos(line2, 0);
  //ucg.print(F("Partition:        "));
  itoa(initPartition, chrdata, 16);
  //ucg.setPrintPos(line2, 100);
  //ucg.setColor(RGB_BLUE);
  //ucg.print(chrdata);

#ifdef DEBUGLOG
  Serial.print(F("Boot partition: "));
  Serial.print(initPartition, DEC);
#endif

  digitalWrite(pinStatusLED, HIGH);

  //HCF
  while (1)
    ;
  
}

//*****************************************************************************
// Function: init_SD_card
// Parameters: none
// Returns: Error
//
// Description: Init card & print informations about the ATA dispositive and the
// FAT File System
//*****************************************************************************
int init_SD_card(void) {
  //int i = 0;
  uint8_t status;
  
  //ucg.setColor(RGB_WHITE);
  //ucg.setPrintPos(line3, 0);
  //ucg.print(F("Card init: "));

  //Init SD library
  status = sd.init(SPI_QUARTER_SPEED, SD_CS);
#ifdef DEBUGLOG
    Serial.print(F("SD Init: 0x"));
    Serial.println(status, HEX);
#endif
  
  if (status) {
      
    // print the type of card to prove it's working
    //ucg.setColor(RGB_BLUE);

    switch (sd.type()) {
      case SD_CARD_TYPE_SD1:
#ifdef DEBUGLOG
        Serial.println(F("SD1"));
#endif
        //ucg.print(F("SD1"));
        break;
      case SD_CARD_TYPE_SD2:
#ifdef DEBUGLOG
        Serial.println(F("SD2"));
#endif
        //ucg.print(F("SD2"));
        break;
      case SD_CARD_TYPE_SDHC:
#ifdef DEBUGLOG
        Serial.println(F("SDHC"));
#endif
        //ucg.print(F("SDHC"));
        break;
      default:
#ifdef DEBUGLOG
        Serial.println(F("Unknown"));
#endif
        //ucg.print(F("Unknown"));
    }
    return 0;

  } else {

#ifdef DEBUGLOG
    Serial.println(F("Error! Card failed to initalise."));
#endif

    //ucg.setColor(RGB_RED);
    //ucg.print(F("Error!"));

    led_err();
    return 1;
    
  }
}

//*****************************************************************************
// Function: led_err
// Parameters: none
// Returns: nothing
//
// Description: Flashes status led for show error status
//
//*****************************************************************************
void led_err(void) {
  int i = 0;
  interrupts();

  for (i = 0; i < 5; i++) {
    digitalWrite(pinStatusLED, HIGH);
    delay(1000);
    digitalWrite(pinStatusLED, LOW);
    delay(1000);
  }
}


//*****************************************************************************
// Function: mcuInit
// Parameters: none
// Returns: none
//
// Description: Initialize the Arduino
//*****************************************************************************
void mcuInit(void) {
  // Input/Output Ports initialization

  //Set ACK pin high and as output
  //PINF = pinACK;
  dirAckOut;

  //TODO: Maybe use defines here for the pins, or some other method.

  //Set rddata and wrdata high, and as input
  PIND =  _BV(7);
  PINE  = _BV(6);
  
  DDRD &= ~(_BV(7));
  DDRE &= ~(_BV(6));


  // Analog Comparator initialization
  // Analog Comparator: Off
  // Analog Comparator Input Capture by Timer/Counter 1: Off
  // Analog Comparator Output: Off
  ACSR = 0x80;

  
  //Organise the in/out pins
  pinMode(pinEject, INPUT);
  pinMode(pinStatusLED, OUTPUT);

  //pinMode(TFT_CS,OUTPUT);
  //pinMode(TFT_DC,OUTPUT);
  //pinMode(TFT_RST,OUTPUT);
}
