# Time font notes

Takeaways from research on font handling for the Pebble Time 2 (Emery), and how
they map to this watchface's current setup.

## Hard constraints (Pebble C SDK)

- **No runtime vector scaling.** Custom TTFs are rasterized at *build time* at the
  size in the resource name's numeric suffix. To have N sizes you register the
  same `.ttf` N times. There is no "draw this TTF at 47px" call at runtime.
- **~48px recommended max, ~60px practical max** for a custom font resource before
  you must switch to per-digit bitmaps. The hard wall is the SDK glyph-dimension
  cap: **256px on aplite/basalt/chalk/diorite, 320px on emery**. At 64px
  Rajdhani's `W` is 257px wide and breaks the build on the small platforms, and at
  72px its `W` is 322px and breaks even emery. Our largest is 64 (`FONT_TIME_64`),
  which builds only because that tier's `characterRegex` omits `w`/`W` (no
  Norwegian/Danish/Swedish word uses them); `M`/`m` are under the cap at 64px.
- **Measurement API** is the tool for fitting:
  `graphics_text_layout_get_content_size(text, font, box, overflow, align)`
  returns the rendered size, so you can measure before drawing.
- **Emery budget is generous:** 256 KB resources, 128 KB app. Bundling several
  font sizes is cheap here (it was painful on 24 KB Aplite). Build output shows
  ~8 KB RAM footprint and ~123 KB free heap on Emery.

## What this face already does (and the research endorses)

- **Bundle discrete sizes + measure-then-choose.** The size ladder (64 -> 20) is
  loaded largest-first and the fitter returns the first that fits. We measure the
  real layout, so fitting is accurate — we do not guess from character count.
- **Line-count-aware worded fit.** `fit_worded_time()` decides line count *before*
  size: it tries the whole phrase on one line (largest font whose true,
  width-unbounded measure fits width-2px and the vertical band); failing that it
  picks the **balanced two-line split** (`balanced_break()` chooses the word break
  that minimizes the wider line, leaving the most room to grow) and takes the
  largest font that fills it; failing that it falls back to natural word-wrap at
  the smallest font. The chosen split is baked into the buffer as a `\n`, so the
  text engine renders exactly the layout we measured. The whole ladder is
  available to every mode — there is no longer a reserved oversized band.
- **Measure width-unbounded.** `layout_fits()` / `balanced_break()` measure in a
  2000px-wide box so the layout breaks only on our explicit `\n`, never on width.
  A `max_w`-wide measuring box would let an over-wide line silently wrap and
  report a deceptively small width, fooling the fitter into accepting it.
- **Recompute vertical origin each tick** from the measured height
  (`update_layout()`), so the time stays centered as the chosen size changes. Both
  worded and single-line modes fit against the real band between panels
  (`time_band_height()`), not a fixed layer height.
- **6px side inset on rectangular** (`layer_inset`), plus a 2px in-layer
  `TIME_SIDE_MARGIN`, so the longest line grows to ~8px of the edge. Round keeps a
  22px inset for the circular bezel.
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
- **`characterRegex` digit subsetting** of the large sizes. Mostly can't apply:
  the same ladder renders worded time, so the resources need letters too. The one
  exception in use is the 64px tier, which drops only `w`/`W` — not to save flash
  but because those two glyphs exceed the SDK's 256px cap at that size and no
  supported language uses them.
- **Trimming the size ladder.** The report calls 2-3 sizes the sweet spot and 5+
  an anti-pattern, but that is about flash cost, which is a non-issue on Emery's
  256 KB. We keep 9 sizes (64 -> 20) for smoother auto-fit, especially for the
  two-line worded layout. (Same Rajdhani TTF reused 9x.)

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
