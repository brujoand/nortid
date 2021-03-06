#include "time2words.h"
#include "string.h"

static const char* const ONES[] = {
  "null",
  "ett",
  "to",
  "tre",
  "fire",
  "fem",
  "seks",
  "syv",
  "åtte",
  "ni"
};

static const char* const TEENS[] ={
  "",
  "elleve",
  "tolv",
  "tretten",
  "fjorten",
  "femten",
  "seksten",
  "sytten",
  "atten",
  "nitten"
};

static const char* const TENS[] = {
  "",
  "ti",
  "tyve",
  "tredve",
  "førti",
  "femti",
  "seksti",
  "sytti",
  "åtti",
  "nitti"
};

static const char* STR_QUARTER = "kvart";
static const char* STR_TO = "på";
static const char* STR_HALF = "halv";
static const char* STR_PAST = "over";

static size_t append_number(char* words, int num) {
  int tens_val = num / 10 % 10;
  int ones_val = num % 10;

  size_t len = 0;

  if (tens_val > 0) {
    if (tens_val == 1 && num != 10) {
      strcat(words, TEENS[ones_val]);
      return strlen(TEENS[ones_val]);
    }
    strcat(words, TENS[tens_val]);
    len += strlen(TENS[tens_val]);
  }

  if (ones_val > 0 || num == 0) {
    strcat(words, ONES[ones_val]);
    len += strlen(ONES[ones_val]);
  }
  return len;
}

static size_t append_string(char* buffer, const size_t length, const char* str) {
  strncat(buffer, str, length);

  size_t written = strlen(str);
  return (length > written) ? written : length;
}

void fuzzy_time_to_words(int hours, int minutes, char* words, size_t length) {
  int fuzzy_hours = hours;
  int fuzzy_minutes = minutes;

  size_t remaining = length;
  memset(words, 0, length);
  if (fuzzy_minutes == 0){
    // Do nothing, omg omg omg
  } else if (fuzzy_minutes < 15) {
    remaining -= append_number(words, fuzzy_minutes);
    remaining -= append_string(words, remaining, " ");
    remaining -= append_string(words, remaining, STR_PAST);
    remaining -= append_string(words, remaining, " ");
  } else if (fuzzy_minutes == 15) {
    remaining -= append_string(words, remaining, STR_QUARTER);
    remaining -= append_string(words, remaining, " ");
    remaining -= append_string(words, remaining, STR_PAST);
    remaining -= append_string(words, remaining, " ");
  } else if (fuzzy_minutes < 30) {
    remaining -= append_number(words, 30 - fuzzy_minutes );
    remaining -= append_string(words, remaining, " ");
    remaining -= append_string(words, remaining, STR_TO);
    remaining -= append_string(words, remaining, " ");
    remaining -= append_string(words, remaining, STR_HALF); 
    remaining -= append_string(words, remaining, " ");
  } else if (fuzzy_minutes == 30) {
    remaining -= append_string(words, remaining, STR_HALF);      
    remaining -= append_string(words, remaining, " ");
  } else if (fuzzy_minutes < 45) {
    remaining -= append_number(words, fuzzy_minutes - 30);
    remaining -= append_string(words, remaining, " ");
    remaining -= append_string(words, remaining, STR_PAST);
    remaining -= append_string(words, remaining, " ");
    remaining -= append_string(words, remaining, STR_HALF);
    remaining -= append_string(words, remaining, " ");
  } else if (fuzzy_minutes == 45) {
    remaining -= append_string(words, remaining, STR_QUARTER);
    remaining -= append_string(words, remaining, " ");
    remaining -= append_string(words, remaining, STR_TO);
    remaining -= append_string(words, remaining, " ");
  } else if (fuzzy_minutes < 60) {
    remaining -= append_number(words, 60 - fuzzy_minutes);
    remaining -= append_string(words, remaining, " ");
    remaining -= append_string(words, remaining, STR_TO);
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

  remaining -= append_number(words, fuzzy_hours);
  
}
