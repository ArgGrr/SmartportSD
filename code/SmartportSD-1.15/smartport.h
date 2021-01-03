#ifndef HEADER_SMARTPORT
  #define HEADER_SMARTPORT

	void encode_status_dib_reply_packet(unsigned char* packet_buffer, unsigned char source);
	void encode_status_reply_packet(unsigned char* packet_buffer, unsigned char source);
	void encode_write_status_packet(unsigned char* packet_buffer, unsigned char source, unsigned char status) ;
	void encode_init_reply_packet(unsigned char* packet_buffer, unsigned char source, unsigned char status);

	void encode_data_packet(unsigned char* packet_buffer, unsigned char* sector_buffer, unsigned char source);
	int decode_data_packet(unsigned char* packet_buffer, unsigned char* sector_buffer);

	int verify_cmdpkt_checksum(unsigned char* packet_buffer);

	int packet_length(unsigned char *packet_buffer);

	void print_packet(unsigned char *data, int bytes);

#endif

