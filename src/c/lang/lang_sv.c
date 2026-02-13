#include "lang.h"

static const char* const ONES_SV[] = {"noll", "ett", "två", "tre",  "fyra",
                                      "fem",  "sex", "sju", "åtta", "nio"};

static const char* const TEENS_SV[] = {"",       "elva",   "tolv",    "tretton", "fjorton",
                                       "femton", "sexton", "sjutton", "arton",   "nitton"};

static const char* const TENS_SV[] = {"",       "tio",    "tjugo",   "trettio", "fyrtio",
                                      "femtio", "sextio", "sjuttio", "åttio",   "nittio"};

static const char* const MONTHS_SV[] = {"Jan", "Feb", "Mar", "Apr", "Maj", "Jun",
                                        "Jul", "Aug", "Sep", "Okt", "Nov", "Dec"};

static const char* const DAYS_SV[] = {"Sön", "Mån", "Tis", "Ons", "Tors", "Fre", "Lör"};

static const TimeStrings TIME_STRINGS_SV = {.ones = ONES_SV,
                                            .teens = TEENS_SV,
                                            .tens = TENS_SV,
                                            .str_quarter = "kvart",
                                            .str_to = "i",
                                            .str_half = "halv",
                                            .str_past = "över"};

static const DateStrings DATE_STRINGS_SV = {.months = MONTHS_SV, .days = DAYS_SV};

const TimeStrings* get_time_strings_sv(void) { return &TIME_STRINGS_SV; }
const DateStrings* get_date_strings_sv(void) { return &DATE_STRINGS_SV; }
