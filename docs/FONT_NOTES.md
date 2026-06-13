# Time font notes

Takeaways from research on font handling for the Pebble Time 2 (Emery), and how
they map to this watchface's current setup.

## Hard constraints (Pebble C SDK)

- **No runtime vector scaling.** Custom TTFs are rasterized at *build time* at the
  size in the resource name's numeric suffix. To have N sizes you register the
  same `.ttf` N times. There is no "draw this TTF at 47px" call at runtime.
- **~48px recommended max, ~60px practical max** for a custom font resource before
  you must switch to per-digit bitmaps. Our largest is 56 (`FONT_TIME_56`).
- **Measurement API** is the tool for fitting:
  `graphics_text_layout_get_content_size(text, font, box, overflow, align)`
  returns the rendered size, so you can measure before drawing.
- **Emery budget is generous:** 256 KB resources, 128 KB app. Bundling several
  font sizes is cheap here (it was painful on 24 KB Aplite). Build output shows
  ~8 KB RAM footprint and ~123 KB free heap on Emery.

## What this face already does (and the research endorses)

- **Bundle discrete sizes + measure-then-choose.** `select_time_font()` in
  `src/c/nortid.c` loops the size ladder largest -> smallest and returns the first
  whose *measured* size fits the box. This is the "choose_font()" pattern the
  report says you largely have to write yourself. We measure the real layout, so
  fitting is accurate — we do not guess from character count.
- **Recompute vertical origin each tick** from the measured height
  (`update_layout()`), so the time stays centered as the chosen size changes.
- **Word-wrap for worded time, single-line for numeric.** `single_line()` gates
  `GTextOverflowModeTrailingEllipsis` vs `GTextOverflowModeWordWrap`, and the size
  ladder reserves the oversized entries (>32px) for single-line numeric mode
  (`TIME_FONT_WRAP_START`).
- **Emery uses a larger value font** for the top metric tiles (Gothic 24 vs 18),
  matching the report's "Emery uses larger fonts by default" guidance.

## Deliberately not done

- **Monospacing / tabular figures.** Rajdhani-Bold's digits are proportional
  (a `1` advances ~334 units, an `8` ~542). The report flags tabular figures as
  "critical for a clock" for two reasons: predictable width-based fitting, and
  avoiding horizontal "wiggle" as digits change.
  - *Predictable fitting:* not needed here — we **measure** the actual rendered
    layout instead of predicting from string length, so proportional digits do
    not break fitting.
  - *Wiggle:* the only real remaining cost. The numeric time can shift a few
    pixels horizontally as digits change each minute. Considered cosmetic and
    left as-is. If it ever needs fixing, the cheapest options are: tabular-figure
    only Rajdhani (equal advance for `0-9 :`, letters untouched, via a fontTools
    build step), or switch numeric mode to a built-in tabular font
    (`LECO_60_NUMBERS` on Emery — free, tabular, numerals-only).
- **`characterRegex` digit subsetting** of the large sizes. Can't apply: the same
  size ladder renders worded time, so the resources need letters too. Subsetting
  would only help if numeric used a separate font.
- **Trimming the 8-size ladder.** The report calls 2-3 sizes the sweet spot and 5+
  an anti-pattern, but that is about flash cost, which is a non-issue on Emery's
  256 KB. We keep 8 sizes for smoother auto-fit, especially for multi-line worded
  time. (Same Rajdhani TTF reused 8x.)

## If a distinct look or bigger-than-60px is ever wanted

- **Built-in tabular numerals:** `LECO_60_NUMBERS_AM_PM` (Emery+ only), `LECO_42`,
  Bitham/Roboto numeric subsets — free, zero flash, tabular, numerals-only.
- **Custom tabular TTF:** DSEG7 (7-segment, SIL OFL) or Roboto Mono (Apache),
  subset to `[0-9: ]`.
- **Bigger than ~60px or animated:** per-digit `GBitmap` drawing (the
  `pebble-examples/big-time` pattern), or a 1-bit glyph atlas integer-upscaled
  per platform (the "Brutal" face upscales 5x on older platforms, 7x on Emery).
- **True continuous scaling** only exists in the Alloy/Moddable JS SDK
  (Emery + Gabbro only); not available from C.
