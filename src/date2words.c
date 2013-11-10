#include "date2words.h"
#include "string.h"
#include "stdio.h"

static const char* const MONTS[] = {
  "Jan",
  "Feb",
  "Mar",
  "Apr",
  "Mai",
  "Jun",
  "Jul",
  "Aug",
  "Sep",
  "Okt",
  "Nov",
  "Des"
};

static const char* const DAYS[] ={
  "Søn",
  "Man",
  "Tirs",
  "Ons",
  "Tors",
  "Fre",
  "Lør"
};

static size_t append_string(char* buffer, const size_t length, const char* str) {
  strncat(buffer, str, length);

  size_t written = strlen(str);
  return (length > written) ? written : length;
}

void date_to_words(int day, int month, int weekday, char date_text[]) {

  size_t remaining = strlen(date_text);
  memset(date_text, 0, remaining);
  char tmp[15];
  snprintf(tmp, 10, "%d", day);

  append_string(date_text, remaining, DAYS[weekday]); 
  append_string(date_text, remaining, " ");
  append_string(date_text, remaining, tmp);     
  append_string(date_text, remaining, " ");
  append_string(date_text, remaining, MONTS[month]);    
}
