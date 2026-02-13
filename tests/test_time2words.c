#include "../src/c/time2words.h"
#include "unity/src/unity.h"

void setUp(void) {}
void tearDown(void) {}

static void assert_time(Language lang, int hours, int minutes, const char* expected) {
  char buffer[64];
  fuzzy_time_to_words(lang, hours, minutes, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING(expected, buffer);
}

void test_exact_hours(void) {
  assert_time(LANG_NO, 1, 0, "ett");
  assert_time(LANG_NO, 2, 0, "to");
  assert_time(LANG_NO, 3, 0, "tre");
  assert_time(LANG_NO, 4, 0, "fire");
  assert_time(LANG_NO, 5, 0, "fem");
  assert_time(LANG_NO, 6, 0, "seks");
  assert_time(LANG_NO, 7, 0, "syv");
  assert_time(LANG_NO, 8, 0, "åtte");
  assert_time(LANG_NO, 9, 0, "ni");
  assert_time(LANG_NO, 10, 0, "ti");
  assert_time(LANG_NO, 11, 0, "elleve");
  assert_time(LANG_NO, 12, 0, "tolv");
}

void test_minutes_past(void) {
  assert_time(LANG_NO, 3, 5, "fem over tre");
  assert_time(LANG_NO, 3, 10, "ti over tre");
  assert_time(LANG_NO, 3, 14, "fjorten over tre");
}

void test_quarter_past(void) { assert_time(LANG_NO, 3, 15, "kvart over tre"); }

void test_minutes_to_half(void) {
  assert_time(LANG_NO, 3, 20, "ti på halv fire");
  assert_time(LANG_NO, 3, 25, "fem på halv fire");
}

void test_half(void) { assert_time(LANG_NO, 3, 30, "halv fire"); }

void test_minutes_past_half(void) {
  assert_time(LANG_NO, 3, 35, "fem over halv fire");
  assert_time(LANG_NO, 3, 40, "ti over halv fire");
}

void test_quarter_to(void) { assert_time(LANG_NO, 3, 45, "kvart på fire"); }

void test_minutes_to(void) {
  assert_time(LANG_NO, 3, 50, "ti på fire");
  assert_time(LANG_NO, 3, 55, "fem på fire");
}

void test_24_hour_conversion(void) {
  assert_time(LANG_NO, 15, 0, "tre");
  assert_time(LANG_NO, 23, 0, "elleve");
  assert_time(LANG_NO, 13, 30, "halv to");
}

void test_midnight(void) { assert_time(LANG_NO, 0, 0, "tolv"); }

void test_noon(void) { assert_time(LANG_NO, 12, 0, "tolv"); }

void test_danish_basics(void) {
  assert_time(LANG_DA, 3, 0, "tre");
  assert_time(LANG_DA, 3, 15, "kvart over tre");
  assert_time(LANG_DA, 3, 30, "halv fire");
  assert_time(LANG_DA, 3, 45, "kvart i fire");
}

void test_swedish_basics(void) {
  assert_time(LANG_SV, 3, 0, "tre");
  assert_time(LANG_SV, 3, 15, "kvart över tre");
  assert_time(LANG_SV, 3, 30, "halv fyra");
  assert_time(LANG_SV, 3, 45, "kvart i fyra");
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_exact_hours);
  RUN_TEST(test_minutes_past);
  RUN_TEST(test_quarter_past);
  RUN_TEST(test_minutes_to_half);
  RUN_TEST(test_half);
  RUN_TEST(test_minutes_past_half);
  RUN_TEST(test_quarter_to);
  RUN_TEST(test_minutes_to);
  RUN_TEST(test_24_hour_conversion);
  RUN_TEST(test_midnight);
  RUN_TEST(test_noon);
  RUN_TEST(test_danish_basics);
  RUN_TEST(test_swedish_basics);
  return UNITY_END();
}
