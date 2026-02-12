#include "../src/date2words.h"
#include "unity/src/unity.h"

void setUp(void) {}
void tearDown(void) {}

static void assert_date(int day, int month, int weekday, const char* expected) {
  char buffer[64];
  date_to_words(day, month, weekday, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING(expected, buffer);
}

void test_sunday(void) { assert_date(1, 0, 0, "Søn 1 Jan"); }

void test_monday(void) { assert_date(15, 2, 1, "Man 15 Mar"); }

void test_tuesday(void) { assert_date(20, 5, 2, "Tirs 20 Jun"); }

void test_wednesday(void) { assert_date(25, 8, 3, "Ons 25 Sep"); }

void test_thursday(void) { assert_date(10, 11, 4, "Tors 10 Des"); }

void test_friday(void) { assert_date(17, 4, 5, "Fre 17 Mai"); }

void test_saturday(void) { assert_date(31, 9, 6, "Lør 31 Okt"); }

void test_all_months(void) {
  assert_date(1, 0, 1, "Man 1 Jan");
  assert_date(1, 1, 1, "Man 1 Feb");
  assert_date(1, 2, 1, "Man 1 Mar");
  assert_date(1, 3, 1, "Man 1 Apr");
  assert_date(1, 4, 1, "Man 1 Mai");
  assert_date(1, 5, 1, "Man 1 Jun");
  assert_date(1, 6, 1, "Man 1 Jul");
  assert_date(1, 7, 1, "Man 1 Aug");
  assert_date(1, 8, 1, "Man 1 Sep");
  assert_date(1, 9, 1, "Man 1 Okt");
  assert_date(1, 10, 1, "Man 1 Nov");
  assert_date(1, 11, 1, "Man 1 Des");
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_sunday);
  RUN_TEST(test_monday);
  RUN_TEST(test_tuesday);
  RUN_TEST(test_wednesday);
  RUN_TEST(test_thursday);
  RUN_TEST(test_friday);
  RUN_TEST(test_saturday);
  RUN_TEST(test_all_months);
  return UNITY_END();
}
