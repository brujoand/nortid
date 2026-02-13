#pragma once

#include <stddef.h>

#include "lang/lang.h"

void date_to_words(Language lang, int day, int month, int weekday, char* words, size_t length);
