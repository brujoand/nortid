#include "lang.h"

static const char* const ONES_NO[] = {"null", "ett",  "to",  "tre",  "fire",
                                      "fem",  "seks", "syv", "åtte", "ni"};

static const char* const TEENS_NO[] = {"",       "elleve",  "tolv",   "tretten", "fjorten",
                                       "femten", "seksten", "sytten", "atten",   "nitten"};

static const char* const TENS_NO[] = {"",      "ti",     "tyve",  "tredve", "førti",
                                      "femti", "seksti", "sytti", "åtti",   "nitti"};

static const char* const MONTHS_NO[] = {"Jan", "Feb", "Mar", "Apr", "Mai", "Jun",
                                        "Jul", "Aug", "Sep", "Okt", "Nov", "Des"};

static const char* const DAYS_NO[] = {"Søn", "Man", "Tirs", "Ons", "Tors", "Fre", "Lør"};

static const TimeStrings TIME_STRINGS_NO = {.ones = ONES_NO,
                                            .teens = TEENS_NO,
                                            .tens = TENS_NO,
                                            .str_quarter = "kvart",
                                            .str_to = "på",
                                            .str_half = "halv",
                                            .str_past = "over"};

static const DateStrings DATE_STRINGS_NO = {.months = MONTHS_NO, .days = DAYS_NO};

const TimeStrings* get_time_strings_no(void) { return &TIME_STRINGS_NO; }
const DateStrings* get_date_strings_no(void) { return &DATE_STRINGS_NO; }
