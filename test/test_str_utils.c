#include "unity/unity.h"
#include "../core/str_utils/str_utils.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_str_copy_safe_normal(void)
{
    char buf[16];
    str_copy_safe(buf, "hello", sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("hello", buf);
}

void test_str_copy_safe_truncates(void)
{
    char buf[4];
    str_copy_safe(buf, "abcdefgh", sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("abc", buf);
}

void test_str_copy_safe_null_src(void)
{
    char buf[8] = "garbage";
    str_copy_safe(buf, NULL, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_str_copy_safe_null_dest(void)
{
    str_copy_safe(NULL, "test", 8);
}

void test_str_contains_found(void)
{
    const u8 data[] = "ELM327 v1.5 OK";
    TEST_ASSERT_TRUE(str_contains(data, 14U, "OK"));
    TEST_ASSERT_TRUE(str_contains(data, 14U, "ELM"));
    TEST_ASSERT_TRUE(str_contains(data, 14U, "327"));
}

void test_str_contains_not_found(void)
{
    const u8 data[] = "NO DATA";
    TEST_ASSERT_FALSE(str_contains(data, 7U, "OK"));
    TEST_ASSERT_FALSE(str_contains(data, 7U, "ERROR"));
}

void test_str_contains_edge_cases(void)
{
    TEST_ASSERT_FALSE(str_contains(NULL, 10U, "test"));
    TEST_ASSERT_FALSE(str_contains((const u8*)"abc", 3U, NULL));
    TEST_ASSERT_FALSE(str_contains((const u8*)"abc", 0U, "a"));
    TEST_ASSERT_FALSE(str_contains((const u8*)"ab", 2U, "abc"));
}

void test_str_contains_exact_match(void)
{
    const u8 data[] = "OK";
    TEST_ASSERT_TRUE(str_contains(data, 2U, "OK"));
}

void test_nibble_to_hex_char(void)
{
    TEST_ASSERT_EQUAL_CHAR('0', nibble_to_hex_char(0U));
    TEST_ASSERT_EQUAL_CHAR('9', nibble_to_hex_char(9U));
    TEST_ASSERT_EQUAL_CHAR('A', nibble_to_hex_char(10U));
    TEST_ASSERT_EQUAL_CHAR('F', nibble_to_hex_char(15U));
    TEST_ASSERT_EQUAL_CHAR('0', nibble_to_hex_char(16U));
}

void test_hex_char_to_nibble(void)
{
    TEST_ASSERT_EQUAL_UINT8(0U, hex_char_to_nibble('0'));
    TEST_ASSERT_EQUAL_UINT8(9U, hex_char_to_nibble('9'));
    TEST_ASSERT_EQUAL_UINT8(10U, hex_char_to_nibble('A'));
    TEST_ASSERT_EQUAL_UINT8(15U, hex_char_to_nibble('F'));
    TEST_ASSERT_EQUAL_UINT8(10U, hex_char_to_nibble('a'));
    TEST_ASSERT_EQUAL_UINT8(15U, hex_char_to_nibble('f'));
    TEST_ASSERT_EQUAL_UINT8(0xFFU, hex_char_to_nibble('G'));
    TEST_ASSERT_EQUAL_UINT8(0xFFU, hex_char_to_nibble(' '));
}

void test_is_printable_ascii(void)
{
    TEST_ASSERT_TRUE(is_printable_ascii(' '));
    TEST_ASSERT_TRUE(is_printable_ascii('~'));
    TEST_ASSERT_TRUE(is_printable_ascii('A'));
    TEST_ASSERT_FALSE(is_printable_ascii(0x00U));
    TEST_ASSERT_FALSE(is_printable_ascii(0x1FU));
    TEST_ASSERT_FALSE(is_printable_ascii(0x7FU));
    TEST_ASSERT_FALSE(is_printable_ascii(0xFFU));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_str_copy_safe_normal);
    RUN_TEST(test_str_copy_safe_truncates);
    RUN_TEST(test_str_copy_safe_null_src);
    RUN_TEST(test_str_copy_safe_null_dest);
    RUN_TEST(test_str_contains_found);
    RUN_TEST(test_str_contains_not_found);
    RUN_TEST(test_str_contains_edge_cases);
    RUN_TEST(test_str_contains_exact_match);
    RUN_TEST(test_nibble_to_hex_char);
    RUN_TEST(test_hex_char_to_nibble);
    RUN_TEST(test_is_printable_ascii);

    return UNITY_END();
}
