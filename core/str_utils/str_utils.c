#include "str_utils.h"

void str_copy_safe(char* dest, const char* src, size_t max_len)
{
    if ((dest == NULL_PTR) || (max_len == 0U)) {
        return;
    }
    
    if (src == NULL_PTR) {
        dest[0] = '\0';
        return;
    }
    
    size_t i = 0U;
    while ((i < (max_len - 1U)) && (src[i] != '\0')) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

bool str_contains(const u8* data, u16 length, const char* pattern)
{
    if ((data == NULL_PTR) || (pattern == NULL_PTR) || (length == 0U)) {
        return false;
    }
    
    size_t pattern_len = 0U;
    while (pattern[pattern_len] != '\0') {
        pattern_len++;
    }
    
    if (pattern_len > length) {
        return false;
    }
    
    for (u16 i = 0U; i <= (length - (u16)pattern_len); i++) {
        bool match = true;
        for (size_t j = 0U; j < pattern_len; j++) {
            if (data[i + j] != (u8)pattern[j]) {
                match = false;
                break;
            }
        }
        if (match == true) {
            return true;
        }
    }
    
    return false;
}

char nibble_to_hex_char(u8 nibble)
{
    if (nibble < 10U) {
        return (char)('0' + nibble);
    }
    if (nibble < 16U) {
        return (char)('A' + (nibble - 10U));
    }
    return '0';
}

u8 hex_char_to_nibble(char c)
{
    if ((c >= '0') && (c <= '9')) {
        return (u8)(c - '0');
    }
    if ((c >= 'A') && (c <= 'F')) {
        return (u8)(c - 'A' + 10);
    }
    if ((c >= 'a') && (c <= 'f')) {
        return (u8)(c - 'a' + 10);
    }
    return 0xFFU;
}

bool is_printable_ascii(u8 c)
{
    return ((c >= 0x20U) && (c <= 0x7EU));
}