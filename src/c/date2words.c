#include "date2words.h"

#include <stdio.h>
#include <string.h>

static const char* const MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "Mai", "Jun",
                                     "Jul", "Aug", "Sep", "Okt", "Nov", "Des"};

static const char* const DAYS[] = {"Søn", "Man", "Tirs", "Ons", "Tors", "Fre", "Lør"};

static size_t append_string(char* buffer, size_t remaining, const char* str) {
  if (remaining <= 1) return 0;
  size_t current_len = strlen(buffer);
  size_t str_len = strlen(str);
  size_t space = remaining - 1;
  size_t to_copy = (str_len < space) ? str_len : space;
  strncat(buffer + current_len, str, to_copy);
  return to_copy;
}

void date_to_words(int day, int month, int weekday, char* words, size_t length) {
  size_t remaining = length;
  memset(words, 0, length);
  char tmp[12];
  snprintf(tmp, sizeof(tmp), "%d", day);

  remaining -= append_string(words, remaining, DAYS[weekday]);
  remaining -= append_string(words, remaining, " ");
  remaining -= append_string(words, remaining, tmp);
  remaining -= append_string(words, remaining, " ");
  append_string(words, remaining, MONTHS[month]);
}
