#include "date2words.h"
#include "string.h"
#include "stdio.h"

static const char* const MONTS[] = {
  "Jan",
  "Feb",
  "Mar",
  "Apr",
  "Maj",
  "Jun",
  "Jul",
  "Aug",
  "Sep",
  "Okt",
  "Nov",
  "Dec"
};

static const char* const DAYS[] ={
  "Sön",
  "Mån",
  "Tir",
  "Ons",
  "Tor",
  "Fre",
  "Lör"
};

static size_t append_string(char* buffer, const size_t length, const char* str) {
  strncat(buffer, str, length);

  size_t written = strlen(str);
  return (length > written) ? written : length;
}

void date_to_words(int day, int month, int weekday, char* words, size_t length) {

  size_t remaining = length;
  memset(words, 0, length);
  char tmp[15];
  snprintf(tmp, 10, "%d", day);

  append_string(words, remaining, DAYS[weekday]); 
  append_string(words, remaining, " ");
  append_string(words, remaining, tmp);     
  append_string(words, remaining, " ");
  append_string(words, remaining, MONTS[month]);    
}
