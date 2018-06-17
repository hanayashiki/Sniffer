#include "stdafx.h"

void byte_to_hex(char byte, char * s) {
	static char hex_char[] = {
		'0', '1', '2', '3', '4',
		'5', '6', '7', '8', '9',
		'A', 'B', 'C', 'D', 'E', 'F'
	};
	int low4 = byte & 0xf;
	int high4 = (byte & 0xf0) >> 4;
	s[0] = hex_char[high4];
	s[1] = hex_char[low4];
}

void ipv6_addr_to_string(char ipv6_addr[], char string_buffer[]) {
	int j = 0;
	for (int i = 0; i < 8; i++) {
		byte_to_hex(ipv6_addr[2 * i], &string_buffer[j]);
		j += 2;
		byte_to_hex(ipv6_addr[2 * i + 1], &string_buffer[j]);
		j += 2;
		string_buffer[j++] = ':';
	}
	string_buffer[j - 1] = '\0';
}

