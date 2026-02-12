# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Nortid is a Pebble smartwatch watchface that displays time and date in Norwegian text.

## Build Commands

Build with Pebble SDK:
```bash
pebble build
```

Install to connected watch:
```bash
pebble install --phone <IP_ADDRESS>
```

Clean build artifacts:
```bash
pebble clean
```

## Architecture

Single-window Pebble watchface with two text layers (time and date).

**Key files:**
- `src/nortid.c` - Main application: window setup, tick timer subscription, display refresh
- `src/time2words.c` - Converts hours/minutes to Norwegian fuzzy time text
- `src/date2words.c` - Converts date to Norwegian weekday/date/month format

**Data flow:**
1. `main()` initializes window and subscribes to `MINUTE_UNIT` tick events
2. Each minute, `refresh_clock()` is called
3. `fuzzy_time_to_words()` converts current time to Norwegian string
4. `date_to_words()` converts current date to Norwegian string
5. Text layers are updated with the new strings

**Norwegian time format:**
- Uses 12-hour format with fuzzy rounding
- Quarter/half hours and "over"/"på" relative expressions
- Example: 2:30 → "halv tre"
