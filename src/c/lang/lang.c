#include "lang.h"

extern const TimeStrings* get_time_strings_no(void);
extern const DateStrings* get_date_strings_no(void);
extern const TimeStrings* get_time_strings_da(void);
extern const DateStrings* get_date_strings_da(void);
extern const TimeStrings* get_time_strings_sv(void);
extern const DateStrings* get_date_strings_sv(void);

const TimeStrings* get_time_strings(Language lang) {
  switch (lang) {
    case LANG_DA:
      return get_time_strings_da();
    case LANG_SV:
      return get_time_strings_sv();
    case LANG_NO:
    default:
      return get_time_strings_no();
  }
}

const DateStrings* get_date_strings(Language lang) {
  switch (lang) {
    case LANG_DA:
      return get_date_strings_da();
    case LANG_SV:
      return get_date_strings_sv();
    case LANG_NO:
    default:
      return get_date_strings_no();
  }
}
