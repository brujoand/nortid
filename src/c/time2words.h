#pragma once

#include <stddef.h>

#include "lang/lang.h"

void fuzzy_time_to_words(Language lang, int hours, int minutes, char* words, size_t length);
