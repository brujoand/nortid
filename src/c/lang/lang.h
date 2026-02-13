#pragma once

#include <stddef.h>

typedef enum { LANG_NO = 0, LANG_DA = 1, LANG_SV = 2 } Language;

typedef struct {
  const char* const* ones;
  const char* const* teens;
  const char* const* tens;
  const char* str_quarter;
  const char* str_to;
  const char* str_half;
  const char* str_past;
} TimeStrings;

typedef struct {
  const char* const* months;
  const char* const* days;
} DateStrings;

const TimeStrings* get_time_strings(Language lang);
const DateStrings* get_date_strings(Language lang);
