#include "date2words.h"

#include <stdio.h>
#include <string.h>

#include "lang/lang.h"

static size_t append_string(char* buffer, size_t remaining, const char* str) {
  if (remaining <= 1) return 0;
  size_t current_len = strlen(buffer);
  size_t str_len = strlen(str);
  size_t space = remaining - 1;
  size_t to_copy = (str_len < space) ? str_len : space;
  strncat(buffer + current_len, str, to_copy);
  return to_copy;
}

void date_to_words(Language lang, int day, int month, int weekday, char* words, size_t length) {
  const DateStrings* strings = get_date_strings(lang);
  memset(words, 0, length);
  if (!strings || !strings->days || !strings->months) return;
  size_t remaining = length;
  char tmp[12];
  snprintf(tmp, sizeof(tmp), "%d", day);

  remaining -= append_string(words, remaining, strings->days[weekday]);
  remaining -= append_string(words, remaining, " ");
  remaining -= append_string(words, remaining, tmp);
  remaining -= append_string(words, remaining, " ");
  append_string(words, remaining, strings->months[month]);
}
