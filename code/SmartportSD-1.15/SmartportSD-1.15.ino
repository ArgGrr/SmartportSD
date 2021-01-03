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
//        there is a disk in the floppy drive, it will boot first. Otherwise it
//        will boot from the CF
// 1.05 - fix problem with internal disk being always write protected. Code was stuck in
//        receivepacket loop, so wrprot(ACK) was always 1. Added timeout support
//        for receivepacket function, so this now times out and goes back to the
//        main loop for another try.
// 1.1  - added support for being connected after unidisk 3.5. Need some more io pins to
//        support pass through, this is the next step.
// 1.12 - add eeprom storing of boot partition, cycling by eject button (PA3)
// 1.13 - Fixed an issue where the block number was calculated incorrectly, leading to
//        failed operations. Now Total Replay v3 works!
// 1.14 - Added basic OLED support to display info, remove 6 second bootup delay left in from testing, code rework.
// 1.15 - Adjusting ports to work on Arduino Pro Micro.
//      - Reworked for combined TFT & SD Card module.
//
// Apple disk interface is connected as follows:
//                   +-------+
//          TX   1 --+       +--    RAW
//          RX   0 --+   P   +--    GND
//          GND    --+   R   +--    RST
//          GND    --+   O   +--    VCC
// PH0      SDA  2 --+       +-- 21 A3     !EJECT PIN
// PH1      SCL  3 --+   M   +-- 20 A2     STATUS LED
// PH2      A6   4 --+   I   +-- 19 A1     WRPROT
// PH3           5 --+   C   +-- 18 A0     TFT RST
// RDATA    A7   6 --+   R   +-- 15 SCLK   SPICLK
// WDATA         7 --+   O   +-- 14 MISO   SPIMISO
// TFT DC   A8   8 --+       +-- 16 MOSI   SPIMOSI
// TFT CS   A9   9 --+       +-- 10 A10    SD CS
//                   +-------+
//
// Mapping of TFT pins to Arduino pins
//  RST   Reset
//  RS    D/C
//  SDA   DIN
//  SCK   CLK
//  CS    CS
//
// NOTE: This is uses the ata code from the fat/fat32/ata drivers written by
//       Angelo Bannack and Giordano Bruno Wolaniuk and ported to mega32 by
//       Murray Horn.
//
//*****************************************************************************
#include <SD.h>
#include <EEPROM.h>
#include <SPI.h>

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735

#include <string.h>

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdio.h>

#include "smartport.h"

//*****************************************************************************
//   Configure Program, comment out defines to save memory
//   LCD        Enable the TFT screen over SPI.
//   STATUSLED  Enabled the drive status LED.
//   SERIALLOG  Enable the serial port logging.
//*****************************************************************************

#define NUM_PARTITIONS 4 // Number of 32MB Prodos partions supported

//*****************************************************************************
// Hardware Configuration
// Define the Apple2c Smartport Pins, make it easier to change references in code.
// Also need to change assignments in packet_16mhz.S
//*****************************************************************************
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


/*
Set DISABLE_CHIP_SELECT to disable a second SPI device.
For example, with the Ethernet shield, set DISABLE_CHIP_SELECT
to 10 to disable the Ethernet controller.
*/
const int8_t DISABLE_CHIP_SELECT = 0;

//*****************************************************************************
// Function Prototypes
//*****************************************************************************
int init_SD_card(void);
void mcuInit(void);
void led_err(void);
void print_text(const __FlashStringHelper * text, uint16_t forecolour, uint16_t backgroundcolor, uint16_t x = 65535, uint16_t y = 65535);
void print_text(char * text, uint16_t forecolour, uint16_t backgroundcolor, uint16_t x = 65535, uint16_t y = 65535);

extern "C" unsigned char ReceivePacket(unsigned char *); // Receive smartport packet assembler function
extern "C" unsigned char SendPacket(unsigned char *);    // send smartport packet assembler function

//*****************************************************************************
// Globals
//*****************************************************************************

Sd2Card sd;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

unsigned char packet_buffer[768]; // smartport packet buffer
unsigned char sector_buffer[512]; // ata sector data buffer
unsigned char status, packet_byte;
int count, partition, initPartition;
unsigned char device_id[NUM_PARTITIONS]; // to hold assigned device id's for the partitions

unsigned long int block_num;
uint8_t sdstato;

char chrdata[16]; //char to store converted numbers etc for serial/OLED display

bool serialenabled = false; ////Flag to disable serial calls if port fails to init

//Constants for drawing on the TFT

//Set up consts for the colours used to make it easy to configure the theme or whatever
#include "gfx_defs.h" //Defines of all webcolours in adafruit 16bit colour format
const uint16_t colour_menuback = COL_ROYAL_BLUE_WEB;
const uint16_t colour_text_menu = COL_WHITE;
const uint16_t colour_background = COL_BLACK;
const uint16_t colour_text = COL_WHITE;
const uint16_t colour_text_value = COL_SLATE_BLUE;
const uint16_t colour_text_read = COL_SEA_GREEN;
const uint16_t colour_text_write = COL_ORANGE_RED;
const uint16_t colour_text_error = COL_ORANGE_RED;






//*****************************************************************************
// Function: Setup
// Parameters: none
// Returns: void
//
// Description: Start up function for the Arduino.
//*****************************************************************************

void setup() {
	mcuInit();

	//*****************************************************************************
	//Power on drive LED at start of setup
	//*****************************************************************************
	digitalWrite(pinStatusLED, HIGH);

	//*****************************************************************************
	//Setup Serial Port
	//*****************************************************************************
	Serial.begin(9600);
	if (!Serial)
	{
		//Wait for serial port to initalise, esp for USB one in Pro Micro
		delay(500);
		if (!Serial)
			serialenabled = false; //Give up, disable serial writing
	}

	//*****************************************************************************
	// Init LCD screen
	//*****************************************************************************
	tft.initR(INITR_BLACKTAB);      // Init ST7735S chip, black tab
	tft.setTextWrap(true);
	tft.setRotation(1);
	tft.fillScreen(colour_background);  //Screen may start with static over it

	//*****************************************************************************
	// Read settings from EEPROM
	//*****************************************************************************
	initPartition = EEPROM.read(0);

	if (initPartition == 0xFF)
	initPartition = 0;
	initPartition = (initPartition % 4);

	//*****************************************************************************
	// Initialise SD Card
	//*****************************************************************************
	while (1)
	{
		if (init_SD_card())	//Init SD Card
		{
			led_err(); 		//Failed, blink LED for a few seconds and try again
		}
		else break;
	}

	/*
	//SD debug info & storage test, print out block 0. Should have data in it.
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
	*/


	//*****************************************************************************
	//Turn off drive LED at end of init/startup
	//*****************************************************************************
	digitalWrite(pinStatusLED, LOW);
}

//*****************************************************************************
// Function: main loop
// Parameters: none
// Returns: void
//
// Description: Main function for Apple //c Smartport SD
//*****************************************************************************

void loop() {
	unsigned char LBH, LBL, LBN;

	int number_partitions_initialised = 1;
	int noid = 0;

	unsigned char source, status, status_code;

	bool wait_for_reset = false;

	if (serialenabled)	Serial.println(F("loop"));
	

	dirReadIn;
	pinRDATA_INV; // set RD low
	interrupts();

	//*****************************************************************************
	// Outer Loop, allows boot roation
	//*****************************************************************************
	while (1)
	{

		//Draw basic screen layout
		tft.fillRoundRect(0, 0, 160, 18, 5,  colour_menuback);
		print_text(F("SmartportSD v1.15"), colour_text_menu, colour_menuback, 30, 5);


		for (partition = 0; partition < NUM_PARTITIONS; partition++) // clear device_id table
		{
			block_num = 2 + (((partition + initPartition) % 4) * 65536);
			sdstato = sd.readBlock(block_num, (unsigned char *)sector_buffer);
			strncpy(chrdata, sector_buffer+5, 16);
			delay(50);

			switch (partition)
			{
			case 0:
				print_text(F("S5D1 "), colour_text, colour_background, 6,  line5 + partition * 9);
				break;
			case 1:
				print_text(F("S5D2 "), colour_text, colour_background, 6,  line5 + partition * 9);
				break;
			case 2:
				print_text(F("S2D1 "), colour_text, colour_background, 6,  line5 + partition * 9);
				break;
			case 3:
				print_text(F("S2D2 "), colour_text, colour_background, 6,  line5 + partition * 9);
			}
			print_text(chrdata, colour_text_value, colour_background);
		}

		//Indicate device is read-only until initialised.
		// Protection for swapping partitions live.
		print_text(F("Waiting for init..."), colour_text_error, colour_background, 0, line14);


		//*****************************************************************************
		// Main Program Loop
		//*****************************************************************************
		while (1)
		{

			if (digitalRead(pinEject) == LOW)
			break;

			noid = 0;   // reset noid flag
			dirAckIn; 	// set ack (wrprot) to input to avoid clashing with other
			// devices when sp bus is not enabled

			switch pinPhases
			{

				/*
				Bus Reset

				phase lines for smartport bus reset
				ph3=0 ph2=1 ph1=0 ph0=1
				*/
			case 0x05:
				if (serialenabled) Serial.println(F("Reset"));
				// monitor phase lines for reset to clear
				while (pinPhases == 0x05)
				;

				number_partitions_initialised = 1; // reset number of partitions init'd
				noid = 0;                          // to check if needed
				for (partition = 0; partition < NUM_PARTITIONS; partition++) // clear device_id table
					device_id[partition] = 0;
				break;

				/*
				Bus Enable

				phase lines for smartport bus enable
				ph3=1 ph2=x ph1=1 ph0=x
				*/
			case 0x0a:
			case 0x0b:
			case 0x0e:
			case 0x0f:
				if (serialenabled) Serial.println("E"); //this is timing sensitive, so can't print to
				//                                      much here as it takes to long

				noInterrupts();
				dirAckOut; // set ack to output, sp bus is enabled
				if ((status = ReceivePacket((unsigned char *)packet_buffer)))
				{
					interrupts();
					if (serialenabled) Serial.print("~");
					break; // error timeout, break and loop again
				}
				interrupts();
				// lets check if the pkt is for us

				// lets check if the pkt is for us
				if (packet_buffer[14] != 0x85) // if its an init pkt, then assume its for us and continue on
				{
					// else check if its our one of our id's
					for (partition = 0; partition < NUM_PARTITIONS; partition++)
					{
						if (device_id[partition] != packet_buffer[6]) // destination id
						noid++;
					}
					if (noid == NUM_PARTITIONS) // not one of our id's
					{

						if (serialenabled) Serial.println("not ours");

						dirAckIn;        // set ack to input, so lets not interfere
						pinACK_INV; // set ack low, for next time its an output
						while pinACK
						; // wait till low other dev has finished receiving it

						if (serialenabled) Serial.println("a");
						//print_packet ((unsigned char*) packet_buffer,packet_length((unsigned char*) packet_buffer));

						// assume its a cmd packet, cmd code is in byte 14
						// now we need to work out what type of packet and stay out of the way
						switch (packet_buffer[14])
						{
							case 0x80: // is a status cmd
							if (serialenabled) Serial.println("S.");

							case 0x83: // is a format cmd
							if (serialenabled) Serial.println("F.");

							case 0x81: // is a readblock cmd
							while (!pinACK)
							; // wait till high
							if (serialenabled) Serial.print("R");

							while pinACK
							; // wait till low
							if (serialenabled) Serial.print("r");

							while (!pinACK)
							; // wait till high
							if (serialenabled) Serial.println("R.");

							break;
							case 0x82: // is a writeblock cmd
							while (!pinACK)
							; // wait till high
							if (serialenabled) Serial.print("W");

							while pinACK
							; // wait till low
							if (serialenabled) Serial.print("w");

							while (!pinACK)
							; // wait till high
							if (serialenabled) Serial.println("W.");

							while pinACK
							; // wait till low
							if (serialenabled) Serial.print("w");

							while (!pinACK)
							; // wait till high
							if (serialenabled) Serial.println("W.");

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

				if (serialenabled)
				{
					Serial.print("Cmd 0x");
					Serial.println(packet_buffer[14], HEX);
					//print_packet ((unsigned char*) packet_buffer,packet_length((unsigned char*) packet_buffer));
				}

				switch (packet_buffer[14])
				{
				//*****************************************************************************
				// Status Command
				// Returns status information about a particular device.
				//*****************************************************************************
				case 0x80:
					digitalWrite(pinStatusLED, HIGH);
					source = packet_buffer[6];
					for (partition = 0; partition < NUM_PARTITIONS; partition++)
					{ // Check if its one of ours
						if (device_id[partition] == source)
						{ // yes it is, then reply
							status_code = (packet_buffer[17] & 0x7f) | (((unsigned short)packet_buffer[16] << 3) & 0x80);
							if (status_code == 0x03)
							encode_status_dib_reply_packet((unsigned char*) packet_buffer, source);             // if statcode=3, then status with device info block
							else
							encode_status_reply_packet((unsigned char*) packet_buffer, source);
							noInterrupts();
							dirReadOut;                                                           // set rd as output
							status = SendPacket((unsigned char *)packet_buffer);
							dirReadIn;                                                            // set rd back to input so back to tristate
							interrupts();

							if (serialenabled)
							{
								Serial.print(F("sts:"));
								Serial.println(status, HEX);
								if (status == 1)
								Serial.println(F("Packet send error."));
								//print_packet((unsigned char*) packet_buffer, packet_length((unsigned char*) packet_buffer));
							}
							digitalWrite(pinStatusLED, LOW);
						}
					}
					break;

				//*****************************************************************************
				// Read Block Command
				//  Reads one 512-block device from a disk device, and writes it to memory.
				//*****************************************************************************
				case 0x81:
					source = packet_buffer[6];
					LBH = packet_buffer[16];
					LBL = packet_buffer[20];
					LBN = packet_buffer[19];
					for (partition = 0; partition < NUM_PARTITIONS; partition++)
					{ // Check if its one of ours
						if (device_id[partition] == source)
						{ // yes it is, then do the read
							// block num 1st byte
							block_num = (LBN & 0x7f) | (((unsigned short)LBH << 3) & 0x80);
							// block num second byte
							block_num = block_num + (((LBL & 0x7f) | (((unsigned short)LBH << 4) & 0x80)) * 256);
							// partition number indicates which 32mb block we access on the CF
							block_num = block_num + (((partition + initPartition) % 4) * 65536);
							digitalWrite(pinStatusLED, HIGH);
							if (serialenabled)
							{
								Serial.println(F("Readblock Command"));
								Serial.print(F("Drive: 0x"));
								Serial.println(source, HEX);

								Serial.print(F("Block: 0x"));
								Serial.println(block_num, HEX);
							}
							tft.fillRect(153, line5 + partition * 9, 5, 8, colour_text_read);
							//tft.drawChar(153, line5 + partition * 9, 'R', colour_text_read, colour_background, 1);

							sdstato = sd.readBlock(block_num, (unsigned char *)sector_buffer);               // Reading block from SD Card

							if (serialenabled)
							{
								if (!sdstato)
								{
									Serial.print(F("SD Error Code: 0x"));
									Serial.println(sd.errorCode(), HEX);
									Serial.print(F("SD Error Data: 0x"));
									Serial.println(sd.errorData(), HEX);
								}
							}
							tft.fillRect(153, line5 + partition * 9, 5, 8, colour_background);

							encode_data_packet((unsigned char*) packet_buffer, (unsigned char*) sector_buffer, source);

							noInterrupts();
							dirReadOut;                                                                     // set rd as output
							status = SendPacket((unsigned char *)packet_buffer);
							dirReadIn;                                                                      // set rd back to input so back to tristate
							interrupts();
							digitalWrite(pinStatusLED, LOW);
							if (serialenabled)
							{
								Serial.print(F("sts:"));
								Serial.println(status, HEX);
								if (status == 1)
								Serial.println(F("Packet send error."));
								//print_packet ((unsigned char*) packet_buffer,packet_length((unsigned char*) packet_buffer));
								//print_packet ((unsigned char*) sector_buffer,15);
							}
						}
					}
					break;

				//*****************************************************************************
				// Write Block Command
				// Writes one 512-byte block from memory to disk device.
				//*****************************************************************************

				case 0x82:
					source = packet_buffer[6];
					for (partition = 0; partition < NUM_PARTITIONS; partition++)
					{                                                                                   // Check if its one of ours
						if (device_id[partition] == source)
						{                                                                                 // yes it is, then do the write
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
							status = decode_data_packet((unsigned char *) packet_buffer,(unsigned char *) sector_buffer);
							if (status == 0)
							{                                                                               // ok

								if (wait_for_reset)
								{
									if (serialenabled) Serial.println(F("read-only, waiting for drive init."));
									status=6;
								}
								else
								{
									                                            // write block to CF card
									digitalWrite(pinStatusLED, HIGH);
									if (serialenabled)
									{
									Serial.println(F("Writeblock Command"));
									Serial.print(F("Drive: 0x"));
									Serial.println(source, HEX);

									Serial.print(F("Block: 0x"));
									Serial.println(block_num, HEX);
									}
									tft.fillRect(153, line5 + partition * 9, 5, 8, colour_text_write);
									//tft.drawChar(153, line5 + partition * 9, 'W', colour_text_write, colour_background, 1);

									sdstato = sd.writeBlock(block_num, (unsigned char *)sector_buffer);           // Write block to SD Card
									if (!sdstato)
									{
										if (serialenabled) Serial.println(F("SD Write failed!"));
										status = 6;
									}

									tft.fillRect(153, line5 + partition * 9, 5, 8, colour_background);
								}
							}
							                                            // now return status code to host
							encode_write_status_packet((unsigned char*) packet_buffer, source, status);
							noInterrupts();
							dirReadOut;                                                                     // set rd as output
							status = SendPacket((unsigned char *)packet_buffer);
							dirReadIn;                                                                      // set rd back to input so back to tristate
							interrupts();

							if (serialenabled)
							{
								Serial.print(F("sts:"));
								Serial.println(status, HEX);
								if (status == 1)
								Serial.println(F("Packet send error."));
								//print_packet ((unsigned char*) packet_buffer,packet_length((unsigned char*) packet_buffer));
								//print_packet ((unsigned char*) sector_buffer,15);
							}
						}
						digitalWrite(pinStatusLED, LOW);
					}
					break;

				//*****************************************************************************
				// Format Command
				// Prepares all blocks on a block device for reading/writing.
				//*****************************************************************************

				case 0x83:
					source = packet_buffer[6];
					for (partition = 0; partition < NUM_PARTITIONS; partition++)
					{                                                                         // Check if its one of ours
						if (device_id[partition] == source)
						{                                                                       // yes it is, then reply to the format cmd
							encode_init_reply_packet((unsigned char *)packet_buffer, source, 0x80);                               // just send back a successful response
							noInterrupts();
							dirReadOut;                                                           // set rd as output
							status = SendPacket((unsigned char *)packet_buffer);
							dirReadIn;                                                            // set rd back to input so back to tristate
							interrupts();
							if (serialenabled)
							{
								Serial.println(F("Formatting!"));
								Serial.print(F("sts:"));
								Serial.println(status, HEX);
								//            print_packet ((unsigned char*) packet_buffer,packet_length((unsigned char*) packet_buffer));
							}
						}
					}
					break;

				//*****************************************************************************
				//  Init Command
				//    Resets all resident devices.
				//*****************************************************************************

				case 0x85:

					//        if (serialenabled) Serial.println(F("Init CMD"));

					source = packet_buffer[6];
					if (number_partitions_initialised < NUM_PARTITIONS)
					{                                                                         // are all init'd yet
						device_id[number_partitions_initialised - 1] = source;                  // remember source id for partition
						number_partitions_initialised++;
						status = 0x80;                                                          // no, so status=0
					}
					else if (number_partitions_initialised >= NUM_PARTITIONS)
					{                                                                         // the last one
						device_id[number_partitions_initialised - 1] = source;                  // remember source id for partition
						number_partitions_initialised++;
						status = 0xff;                                                          // yes, so status=non zero
					}
					/*
					if (serialenabled)
					{
					Serial.print(F("number_partitions_initialised: 0x"));
					Serial.println(number_partitions_initialised,HEX);

					if (number_partitions_initialised - 1 == NUM_PARTITIONS)
					{
					for (partition = 0; partition < 4; partition++)
					{
					Serial.print(F("Drive: "));
					Serial.println(device_id[partition],HEX);
					}
					}
					}
					*/
					encode_init_reply_packet((unsigned char*) packet_buffer, source, status);
					/*
					if (serialenabled)
					{
					Serial.print(F("Status: 0x"));
					Serial.println(status,HEX);
					Serial.println(F("Reply:"));
					print_packet ((unsigned char*) packet_buffer,packet_length((unsigned char*) packet_buffer));
					}
					*/
					noInterrupts();
					dirReadOut;                                                               // set rd as output
					status = SendPacket((unsigned char *)packet_buffer);
					dirReadIn;                                                                // set rd back to input so back to tristate
					interrupts();

					wait_for_reset = false; //Drives all reset now, good to go.
					tft.fillRect(0, line4, 160, 8, colour_background);
					if (serialenabled)
					{
						Serial.print(F("sts:"));
						Serial.println(status);
						if (number_partitions_initialised - 1 == NUM_PARTITIONS)
						{
							Serial.print(F("Drives:"));
							Serial.println(number_partitions_initialised - 1);
							for (partition = 0; partition < 4; partition++)
							{
								Serial.print(F("Drv:"));
								Serial.println(device_id[partition], HEX);
							}
						}
					}
					break;

				//case 0x84://Control
				//case 0x86://Open
				//case 0x87://Close

				}//switch (packet_buffer[14])
			}//switch pinPhases
		}//Main Program Loop

		//Button pressed, rotate boot and re-read the partition names.
		initPartition++;
		initPartition = initPartition % 4;
		EEPROM.write(0, initPartition);

		if (serialenabled)
		{
			Serial.println(F("Button Pressed, partitions rotated, read-only until init."));
		}

		wait_for_reset = true;  //Don't do any writes until next init after a partition swap
		tft.fillScreen(colour_background);	//Clear screen here, will be re-drawn at start of main loop

	}//Outer Loop
}//loop


//*****************************************************************************
// Function: init_SD_card
// Parameters: none
// Returns: Error
//
// Description: Init card & print informations about the ATA dispositive and the
// FAT File System
//*****************************************************************************
int init_SD_card(void)
{
	uint8_t status;


	print_text(F("Card Init: "), colour_text, colour_background, 0, line13);

	//Init SD library
	// Had to set quarter speed, doesn't work at half speed in this hardware configuration
	status = sd.init(SPI_QUARTER_SPEED, SD_CS);

	if (status)
	{
		// print the type of card to prove it's initialised
		switch (sd.type())
		{
			case SD_CARD_TYPE_SD1:
				print_text(F("SD1    "), colour_text_value, colour_background);
				break;
			case SD_CARD_TYPE_SD2:
				print_text(F("SD2    "), colour_text_value, colour_background);
				break;
			case SD_CARD_TYPE_SDHC:
				print_text(F("SDHC   "), colour_text_value, colour_background);
				break;
			default:
				print_text(F("Unknown"), colour_text_value, colour_background);
		}



		//Return no error
		return 0;
	}
	else
	{
		print_text(F("Error! "), colour_text_error, colour_menuback);
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
	interrupts();

	for (int i = 0; i < 5; i++) {
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

	//Setup the in/out pins
	pinMode(pinEject, INPUT_PULLUP);
	pinMode(pinStatusLED, OUTPUT);
}

//*****************************************************************************
// Function: print_text
// Parameters: text, foreground colour, background colour, x position, y position
// Returns: none
//
// Description: Print a string to the tft at the specifiied location
//					in the specified colour, also send to serial port.
//				if x & y not specified, text will print at current cursor pos.
//*****************************************************************************
void print_text(const __FlashStringHelper* text, uint16_t forecolour, uint16_t backgroundcolor, uint16_t x = 65535, uint16_t y = 65535)
{
	if (x < 65535 || y < 65535)
		tft.setCursor(x, y);
	tft.setTextColor(forecolour, backgroundcolor);
	tft.print(text);

	if (serialenabled) Serial.println(text);
}
void print_text(char* text, uint16_t forecolour, uint16_t backgroundcolor, uint16_t x = 65535, uint16_t y = 65535)
{
	if (x < 65535 || y < 65535)
		tft.setCursor(x, y);
	tft.setTextColor(forecolour, backgroundcolor);
	tft.print(text);

	if (serialenabled) Serial.println(text);
}

