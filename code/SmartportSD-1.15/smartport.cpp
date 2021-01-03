#include <Arduino.h>

//*****************************************************************************
// Function: encode_status_dib_reply_packet
// Parameters: pointer to packet_buffer, source
// Returns: none
//
// Description: this is the reply to the status command 03 packet. The reply
// includes following:
// data byte 1
// data byte 2-4 number of blocks. 2 is the LSB and 4 the MSB. Hard coded for
// 32mb partitions. eg 0x00ffff
//*****************************************************************************
void encode_status_dib_reply_packet(unsigned char* packet_buffer, unsigned char source) {
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
  for (int count = 7; count < 14; count++) // xor the packet header bytes
    checksum = checksum ^ packet_buffer[count];
  packet_buffer[43] = checksum | 0xaa;      // 1 c6 1 c4 1 c2 1 c0
  packet_buffer[44] = checksum >> 1 | 0xaa; // 1 c7 1 c5 1 c3 1 c1

  packet_buffer[45] = 0xc8; // PEND
  packet_buffer[46] = 0x00; // end of packet in buffer
}

//*****************************************************************************
// Function: packet_length
// Parameters: pointer to packet_buffer
// Returns: length
//
// Description: Calculates the length of the packet in the packet_buffer.
// A zero marks the end of the packet data.
//*****************************************************************************
int packet_length(unsigned char* packet_buffer) {
  int x = 0;

  while (packet_buffer[x++])
    ;
  return x - 1; // point to last packet byte = C8
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
  int count, row;
  char tbs[4];
  char he2[2];
  char xx;

#ifdef SERIALLOG
  if (serialenabled)
  {
    for (int count = 0; count < bytes; count = count + 16)
    {
      sprintf(tbs, "%04X: ", count);
      Serial.print(tbs);
      for (row = 0; row < 16; row++)
      {
        if (count + row >= bytes)
          Serial.print(F("   "));
        else
        {
          sprintf(he2, "%02X ", data[count + row]);
          Serial.print(he2);
        }
      }
      Serial.print("-");
      for (row = 0; row < 16; row++) {
        if ((data[count + row] > 31) && (count + row < bytes) && (data[count + row] < 129))
        {
          xx = data[count + row];
          Serial.print(xx);
        }
        else
          Serial.print(("."));
      }
      Serial.print(("\r\n"));
    }
  }
#endif
}


//*****************************************************************************
// Function: encode_data_packet
// Parameters: pointer to packet_buffer, pointer to sector_buffer, source id
// Returns: none
//
// Description: encode 512 byte data packet for read block command from host
// requires the data to be in the sector buffer, and builds the smartport
// packet into the packet buffer
//*****************************************************************************
void encode_data_packet(unsigned char* packet_buffer, unsigned char* sector_buffer, unsigned char source) {
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
  for (int count = 0; count < 512; count++) // xor all the data bytes
    checksum = checksum ^ sector_buffer[count];
  for (int count = 7; count < 14; count++) // now xor the packet header bytes
    checksum = checksum ^ packet_buffer[count];
  packet_buffer[600] = checksum | 0xaa;      // 1 c6 1 c4 1 c2 1 c0
  packet_buffer[601] = checksum >> 1 | 0xaa; // 1 c7 1 c5 1 c3 1 c1

  // end bytes
  packet_buffer[602] = 0xc8; // pkt end
  packet_buffer[603] = 0x00; // mark the end of the packet_buffer
}

//*****************************************************************************
// Function: decode_data_packet
// Parameters: pointer to packet_buffer, pointer to sector_buffer
// Returns: error code, >0 = error encountered
//
// Description: decode 512 byte data packet for write block command from host
// decodes the data from the packet_buffer into the sector_buffer
//*****************************************************************************
int decode_data_packet(unsigned char* packet_buffer, unsigned char* sector_buffer) {
  int grpbyte, grpcount;
  unsigned char checksum = 0, bit0to6, bit7, oddbits, evenbits;

  // add oddbyte, 1 in a 512 data packet
  sector_buffer[0] = ((packet_buffer[13] << 1) & 0x80) | (packet_buffer[14] & 0x7f);

  // 73 grps of 7 in a 512 byte packet
  for (grpcount = 0; grpcount < 73; grpcount++)
  {
    for (grpbyte = 0; grpbyte < 7; grpbyte++)
    {
      bit7 = (packet_buffer[15 + (grpcount * 8)] << (grpbyte + 1)) & 0x80;
      bit0to6 = (packet_buffer[16 + (grpcount * 8) + grpbyte]) & 0x7f;
      sector_buffer[1 + (grpcount * 7) + grpbyte] = bit7 | bit0to6;
    }
  }

  // verify checksum
  for (int count = 0; count < 512; count++) // xor all the data bytes
    checksum = checksum ^ sector_buffer[count];
  for (int count = 6; count < 13; count++) // now xor the packet header bytes
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
// Parameters: pointer to packet_buffer, source, status
// Returns: none
//
// Description: this is the reply to the write block data packet. The reply
// indicates the status of the write block cmd.
//*****************************************************************************
void encode_write_status_packet(unsigned char* packet_buffer, unsigned char source, unsigned char status) {
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

  for (int count = 7; count < 14; count++) // xor the packet header bytes
    checksum = checksum ^ packet_buffer[count];
  packet_buffer[14] = checksum | 0xaa;      // 1 c6 1 c4 1 c2 1 c0
  packet_buffer[15] = checksum >> 1 | 0xaa; // 1 c7 1 c5 1 c3 1 c1

  packet_buffer[16] = 0xc8; // pkt end
  packet_buffer[17] = 0x00; // mark the end of the packet_buffer
}

//*****************************************************************************
// Function: encode_init_reply_packet
// Parameters: pointer to packet_buffer, source, status
// Returns: none
//
// Description: this is the reply to the init command packet. A reply indicates
// the original dest id has a device on the bus. If the STAT byte is 0, (0x80)
// then this is not the last device in the chain. This is written to support up
// to 4 partions, i.e. devices, so we need to specify when we are doing the last
// init reply.
//*****************************************************************************
void encode_init_reply_packet(unsigned char* packet_buffer, unsigned char source, unsigned char status) {
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

  for (int count = 7; count < 14; count++) // xor the packet header bytes
    checksum = checksum ^ packet_buffer[count];
  packet_buffer[14] = checksum | 0xaa;      // 1 c6 1 c4 1 c2 1 c0
  packet_buffer[15] = checksum >> 1 | 0xaa; // 1 c7 1 c5 1 c3 1 c1

  packet_buffer[16] = 0xc8; // PEND
  packet_buffer[17] = 0x00; // end of packet in buffer
}

//*****************************************************************************
// Function: encode_status_reply_packet
// Parameters: pointer to packet_buffer, source
// Returns: none
//
// Description: this is the reply to the status command packet. The reply
// includes following:
// data byte 1
// data byte 2-4 number of blocks. 2 is the LSB and 4 the MSB. Hard coded for
// 32mb partitions. eg 0x00ffff
//*****************************************************************************
void encode_status_reply_packet(unsigned char* packet_buffer, unsigned char source) {
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
  for (int count = 7; count < 14; count++) // xor the packet header bytes
    checksum = checksum ^ packet_buffer[count];
  packet_buffer[19] = checksum | 0xaa;      // 1 c6 1 c4 1 c2 1 c0
  packet_buffer[20] = checksum >> 1 | 0xaa; // 1 c7 1 c5 1 c3 1 c1

  packet_buffer[21] = 0xc8; // PEND
  packet_buffer[22] = 0x00; // end of packet in buffer
}


//*****************************************************************************
// Function: verify_cmdpkt_checksum
// Parameters: pointer to packet_buffer
// Returns: 0 = ok, 1 = error
//
// Description: verify the checksum for command packets
//
// &&&&&&&&not used at the moment, no error checking for checksum for cmd packet
//*****************************************************************************
int verify_cmdpkt_checksum(unsigned char* packet_buffer)
{
  int count = 0, length;
  unsigned char evenbits, oddbits, bit7, bit0to6, grpbyte;
  unsigned char calc_checksum = 0; // initial value is 0
  unsigned char pkt_checksum;

  length = packet_length((unsigned char *)packet_buffer);

  // 2 oddbytes in cmd packet
  calc_checksum ^= ((packet_buffer[13] << 1) & 0x80) | (packet_buffer[14] & 0x7f);
  calc_checksum ^= ((packet_buffer[13] << 2) & 0x80) | (packet_buffer[15] & 0x7f);

  // 1 group of 7 in a cmd packet
  for (grpbyte = 0; grpbyte < 7; grpbyte++)
  {
    bit7 = (packet_buffer[16] << (grpbyte + 1)) & 0x80;
    bit0to6 = (packet_buffer[17 + grpbyte]) & 0x7f;
    calc_checksum ^= bit7 | bit0to6;
  }

  // calculate checksum for overhead bytes
  for (int count = 6; count < 13; count++) // start from first id byte
    calc_checksum ^= packet_buffer[count];

  oddbits = (packet_buffer[length - 2] << 1) | 0x01;
  evenbits = packet_buffer[length - 3];
  pkt_checksum = oddbits | evenbits;

#ifdef SERIALLOG
  if (serialenabled)
  {
    Serial.print(F("Pkt Chksum Byte:\r\n"));
    Serial.println(pkt_checksum, DEC);
    Serial.print(F("Calc Chksum Byte:\r\n"));
    Serial.println(calc_checksum, DEC);
  }
#endif

  if (pkt_checksum == calc_checksum)
    return 1;
  else
    return 0;
}
