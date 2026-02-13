#include "time2words.h"

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

static size_t append_number(char* buffer, size_t remaining, int num, const TimeStrings* strings) {
  int tens_val = num / 10 % 10;
  int ones_val = num % 10;
  size_t written = 0;

  if (tens_val > 0) {
    if (tens_val == 1 && num != 10) {
      return append_string(buffer, remaining, strings->teens[ones_val]);
    }
    written += append_string(buffer, remaining - written, strings->tens[tens_val]);
  }

  if (ones_val > 0 || num == 0) {
    written += append_string(buffer, remaining - written, strings->ones[ones_val]);
  }
  return written;
}

void fuzzy_time_to_words(Language lang, int hours, int minutes, char* words, size_t length) {
  const TimeStrings* strings = get_time_strings(lang);
  int fuzzy_hours = hours;
  int fuzzy_minutes = minutes;

  size_t remaining = length;
  memset(words, 0, length);

  if (fuzzy_minutes > 0 && fuzzy_minutes < 15) {
    remaining -= append_number(words, remaining, fuzzy_minutes, strings);
    remaining -= append_string(words, remaining, " ");
    remaining -= append_string(words, remaining, strings->str_past);
    remaining -= append_string(words, remaining, " ");
  } else if (fuzzy_minutes == 15) {
    remaining -= append_string(words, remaining, strings->str_quarter);
    remaining -= append_string(words, remaining, " ");
    remaining -= append_string(words, remaining, strings->str_past);
    remaining -= append_string(words, remaining, " ");
  } else if (fuzzy_minutes > 15 && fuzzy_minutes < 30) {
    remaining -= append_number(words, remaining, 30 - fuzzy_minutes, strings);
    remaining -= append_string(words, remaining, " ");
    remaining -= append_string(words, remaining, strings->str_to);
    remaining -= append_string(words, remaining, " ");
    remaining -= append_string(words, remaining, strings->str_half);
    remaining -= append_string(words, remaining, " ");
  } else if (fuzzy_minutes == 30) {
    remaining -= append_string(words, remaining, strings->str_half);
    remaining -= append_string(words, remaining, " ");
  } else if (fuzzy_minutes > 30 && fuzzy_minutes < 45) {
    remaining -= append_number(words, remaining, fuzzy_minutes - 30, strings);
    remaining -= append_string(words, remaining, " ");
    remaining -= append_string(words, remaining, strings->str_past);
    remaining -= append_string(words, remaining, " ");
    remaining -= append_string(words, remaining, strings->str_half);
    remaining -= append_string(words, remaining, " ");
  } else if (fuzzy_minutes == 45) {
    remaining -= append_string(words, remaining, strings->str_quarter);
    remaining -= append_string(words, remaining, " ");
    remaining -= append_string(words, remaining, strings->str_to);
    remaining -= append_string(words, remaining, " ");
  } else if (fuzzy_minutes > 45) {
    remaining -= append_number(words, remaining, 60 - fuzzy_minutes, strings);
    remaining -= append_string(words, remaining, " ");
    remaining -= append_string(words, remaining, strings->str_to);
    remaining -= append_string(words, remaining, " ");
  }

  if (fuzzy_minutes > 15) {
    fuzzy_hours = fuzzy_hours + 1;
  }

  if (fuzzy_hours > 12) {
    fuzzy_hours = fuzzy_hours - 12;
  } else if (fuzzy_hours == 0) {
    fuzzy_hours = 12;
  }

  append_number(words, remaining, fuzzy_hours, strings);
}
