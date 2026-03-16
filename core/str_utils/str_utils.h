#ifndef STR_UTILS_H
#define STR_UTILS_H

#include "../types.h"

void str_copy_safe(char* dest, const char* src, size_t max_len);

bool str_contains(const u8* data, u16 length, const char* pattern);

char nibble_to_hex_char(u8 nibble);

u8 hex_char_to_nibble(char c);

bool is_printable_ascii(u8 c);

#endif