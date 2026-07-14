# nortid

Pebble smartwatch watchface showing time and date as Scandinavian words.
C (Pebble SDK) + JS (Clay config).

## Hard rules

- **Never push to `main`/`master`.** Feature branch + PR, always. Open the PR,
  report the URL, stop — **only the human merges.**
- **Conventional Commits** (`feat:`, `fix:`, `chore:`, …). Never hand-bump a version.
- **`pre-commit` is the gate.** Run `pre-commit run --files <changed>` before
  declaring a change done, and report the result.
- **`mise` provisions the toolchain** (`mise install`). New worktrees need `mise trust`.
- Plan every non-trivial task. If the plan fails, restart planning.

## Workflow

Default branch is `master`. A `commit-msg` hook enforces `type(scope)?: description`;
append `!` for breaking changes. The version is derived from git tags and released
automatically on merge — never edit it by hand.

## Architecture

Single-window watchface: a center time text layer, a top panel split into three
configurable metric slots (steps / heart rate / sleep), and a bottom panel that
shows the date when enabled.

- `src/c/nortid.c` — window setup, tick subscription, panel rendering, refresh
- `src/c/time2words.c` — hours/minutes → fuzzy time text
- `src/c/date2words.c` — date → weekday/date/month
- `src/c/lang/` — per-language word tables (Norwegian, Danish, Swedish)
- `src/pkjs/config.json` — Clay settings UI; `package.json` `messageKeys` maps
  config keys to app message keys
- `docs/FONT_NOTES.md` — why a size ladder is bundled and measure-then-fit is used

Flow: `main()` subscribes to `MINUTE_UNIT`; each minute `refresh_clock()` calls
`fuzzy_time_to_words()` and `date_to_words()`, then refreshes the layers.

Norwegian time is 12-hour with fuzzy rounding and "over"/"på" relative
expressions — 2:30 becomes "halv tre".

## Commands

```bash
make test                       # both suites (needs gcc)
make test-time | make test-date # single suite; no finer granularity
make clean
pebble build                    # needs `pip install pebble-tool`
pebble install --phone <IP>
pre-commit run --files <changed files>
```

## Gotchas

- **Unity is vendored, not a submodule.** `tests/unity/` is a byte-exact copy of
  upstream pinned in `tests/unity/README.md`. Never edit it; upgrade by replacing
  the files and updating that README.
- `gitleaks` allowlists pebble-tool's Firebase web API key — it is public.
- Pre-commit also runs `clang-format` (Google style) and `make test`. Install the
  commit-msg hook too: `mise x -- pre-commit install --hook-type commit-msg`.
