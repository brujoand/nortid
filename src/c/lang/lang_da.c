#include "lang.h"

static const char* const ONES_DA[] = {"nul", "et",   "to",  "tre",  "fire",
                                      "fem", "seks", "syv", "otte", "ni"};

static const char* const TEENS_DA[] = {"",       "elleve",  "tolv",   "tretten", "fjorten",
                                       "femten", "seksten", "sytten", "atten",   "nitten"};

static const char* const TENS_DA[] = {"",          "ti",   "tyve",       "tredive", "fyrre",
                                      "halvtreds", "tres", "halvfjerds", "firs",    "halvfems"};

static const char* const MONTHS_DA[] = {"Jan", "Feb", "Mar", "Apr", "Maj", "Jun",
                                        "Jul", "Aug", "Sep", "Okt", "Nov", "Dec"};

static const char* const DAYS_DA[] = {"Søn", "Man", "Tirs", "Ons", "Tors", "Fre", "Lør"};

static const TimeStrings TIME_STRINGS_DA = {.ones = ONES_DA,
                                            .teens = TEENS_DA,
                                            .tens = TENS_DA,
                                            .str_quarter = "kvart",
                                            .str_to = "i",
                                            .str_half = "halv",
                                            .str_past = "over"};

static const DateStrings DATE_STRINGS_DA = {.months = MONTHS_DA, .days = DAYS_DA};

const TimeStrings* get_time_strings_da(void) { return &TIME_STRINGS_DA; }
const DateStrings* get_date_strings_da(void) { return &DATE_STRINGS_DA; }
