# Calculator input: first baby step

Status: **implemented; owner confirmed successful upload and basic operation on PaperMono**.
Baseline confirmed on device. Decimal input, backspace and basic arithmetic added 2026-09-15; device checks for these follow-ups remain pending.
Parent: [PaperMono roadmap](papermono-roadmap.md).

## Goal

Home → Tools → Calculator. Press digits, see the sequence, press `C` to clear it.
The original input screen now supports one pending arithmetic operation; expression parsing remains deferred.

## Where the key design comes from

**Recommendation: reuse the FreeInkUI keyboard renderer already used by CrossPoint.**
Supply numeric rows instead of letter rows; keep the existing key drawing and touch registration.

| Part | Existing source | What we would reuse |
| --- | --- | --- |
| Keys on a real CrossPoint screen | [KeyboardEntryActivity.cpp](../src/activities/util/KeyboardEntryActivity.cpp), lines 953–987 | The `keyboard()` renderer, existing font binding and theme-derived `keyboardKeySpacing`. This is the primary visual reference, not a newly designed calculator skin. |
| Numeric arrangement capability | Pinned FreeInk SDK `7f6bd0f`, `libs/ui/FreeInkUI/include/components/keyboard/keyboard.h`, lines 43–65 and 600–618 | Custom rows and relative key widths are already supported. We supply labels/values; the component computes the key rectangles. |
| Key appearance and hit handling | Same pinned SDK file, lines 503–548 | Default button styles and the existing `button()` component provide drawing, states, and touch targets. No hand-drawn key borders, custom hit rectangles, new fonts, or new radius/color tokens. |
| Screen chrome and input routing | [UiAppHost.h](../src/components/UiAppHost.h), lines 10–21 and 34–60; [Activity.h](../src/activities/Activity.h), lines 31–64 | Existing themed UI hosting and lifecycle/navigation. Input display uses existing theme typography, rather than a custom segmented-digit display. |

The SDK also has `keyGrid()` in `components/keyboard/key-grid.h` (lines 35–84), which delegates to the same
button primitive. The keyboard renderer is the better fit for the proposed uneven final row, without a dummy key.
CrossPlay's `src/apps_local/wikipedia/WikipediaActivity.cpp:307` also uses `keyboard()`; no separate calculator
design was found to copy in the inspected app trees.

The local reference SDK is newer than the fork's pinned SDK: its keyboard defaults differ. Do not use a newer
gallery image as proof of this fork's exact appearance or update the SDK just to obtain that appearance.
The current CrossPoint keyboard and the pinned renderer are the implementation references.

## Agreed arrangement

- Existing header/navigation, one readout above the keypad, no extra toolbar.
- Calculator ordering: `7 8 9`, then `4 5 6`, then `1 2 3`.
- Top row: `C`, `CE`, `Del` (backspace), and `÷`. Digit rows have `×`, `−`, and `+` in the right column.
- Final row: wide `0`, `.`, and `=`.
- All widths and five rows use the existing keyboard component.
- Reuse the current keyboard's visual treatment and theme spacing. Use existing layout metrics and oriented
  viewable bounds for the screen; do not invent fixed pixel coordinates.
- Readout: right-aligned bundled 18pt Noto Sans, initially `0`. No cursor or direct text editing.

These composition choices were agreed before implementation. If the stock appearance does not suit,
discuss the specific difference before changing it.

## Behavior

- Start and clear to `0`. Repeated leading zeros do not accumulate; a nonzero digit replaces the initial zero.
- `.` starts `0.`; reject a second decimal point. Preserve fractional zeros, such as `0.005`.
- A held key does not auto-repeat. Use existing navigation/gesture behavior, with no new gestures.
- Limit: 12 digits excluding the decimal point. At the bound, ignore further digits; `C` remains available.
  A first decimal point can still be added. Verify the longest readout fits on device.
- Input belongs to this screen session. Leaving and reopening starts at `0`; no SD writes or restoration.
- A fixed 14-byte character buffer holds 12 digits, a point and the terminator. Arithmetic uses standard C++ `double`, `strtod`, and `snprintf`; a separate fixed 32-byte buffer holds results.
  There is no added heap allocation or external arithmetic dependency. Labels use the repository's `tr()` convention.
- `+`, `−`, `×`, `÷`, and `=` evaluate sequentially: `2 + 3 × 4 = 20`. An operator pressed before a new
  operand replaces the pending operator. Repeated `=` does nothing; typing after a result starts fresh.
- Results use up to 12 significant digits, trimming trailing zeros and using scientific notation where needed.
  Stored results round to the visible precision. Binary floating point is approximate, not exact decimal arithmetic.
- A reserved text line above the readout shows the pending operator, clearing after equals, all-clear or error.
- `CE` resets only the active entry to `0`, preserving the stored number and pending operation.
  After a result it starts a zero entry; from an error it resets the whole calculator.
- `Del` removes one character from the active operand, returning to `0` when empty. It does not edit results.
- Division by zero or non-finite results show `Error`; `C` or a new digit recovers.

## Out of scope

Negative number entry, expression parsing/precedence, repeated-equals calculations, history, memory buttons,
copy/send, BLE, persistence, custom key artwork, and a reusable calculator/app framework.
No hardware drivers or background tasks are needed for this increment.

## Verification when implemented

- Runnable input check covers default/clear, leading zeros, `0.005`, duplicate decimal rejection,
  the 12-digit bound with and without a decimal, and null termination.
- PaperMono: verify `12 + 99 CE 3 = 15`; the `+` stays visible through entry/CE and clears after `=` or `C`.
  Check the added indicator line and four top-row touch targets in supported orientations.
- PaperMono: repeat those cases, edit `12.5` with `Del`, check `12.5 × 2 = 25`, `1 − 2 = -1`,
  `1 ÷ 3`, operator replacement, sequential operations, error recovery after `1 ÷ 0`, and long scientific results.
- PaperMono: tap every digit, tap quickly during refreshes, hold a key, tap near key boundaries, and confirm
  each deliberate activation produces exactly one digit. Verify clear and the full-length readout.
- Exercise Back → Tools, Home, sleep/wake, and reopen; then reopen a book and verify reading position and input.
- Check the reused layout and touch targets in the reader's supported orientations and the active theme.
  Do not claim the design matches from a host render alone.
- Host checks cover input editing, four operations, chaining, rounding, overflow and error recovery. Device checks remain pending.

## Implementation notes

- Visual follow-up: every key has a 1-pixel black border through the existing component styles. The readout
  uses the already-bundled 18-point Noto Sans, with its line height reserved above the keypad; input remains
  limited to 12 digits. The navigator starts unselected, so opening Calculator does not highlight `7`.
  The PaperMono build passed for this follow-up; its device appearance still needs checking.
- Tools uses the existing `UiListActivity`; Calculator uses `UiAppHost` and the pinned SDK keyboard renderer.
  The Home entry uses the existing folder icon. PaperMono Home paging shares its offset between drawing and
  touch routing so the extra row remains reachable when all rows cannot fit below the cover.
- Activities follow the existing heap-owned lifecycle and use checked `makeUniqueNoThrow` allocation. Home is
  replaced by Tools; Tools stays on the existing activity stack while Calculator is open. This preserves Back
  navigation without introducing another owner/task. Key/layout data lives in the activity, not on the task stack.
- Input check passed: `c++ -std=c++20 test/calculator_input/CalculatorInputTest.cpp -o /tmp/calculator-input-test`
  followed by `/tmp/calculator-input-test`. Repository formatting and whitespace checks passed.
- `pio run -e papermono` passed on 2026-09-15. For this checkout's local tool installation, run
  `PLATFORMIO_CORE_DIR="$PWD/.pio/core" .venv/bin/pio run -e papermono` through RTK.
  The pinned SDK source was populated from its Git archive for the build; no SDK revision was changed.
  The owner subsequently confirmed successful upload and basic operation on PaperMono.

### Responsive input and latest-state feedback

Input state belongs to the input task; rendering takes the latest published snapshot without blocking
key handling on the display-refresh lock. New input replaces the pending visual snapshot, so the next
refresh shows the latest result and all keys pressed since the preceding snapshot, without replaying intermediate results.
A bitmask accumulates key feedback separately from physical-button navigation selection; repeated presses
share one highlight. Input arriving during refresh remains pending for the next snapshot.
The display refreshes sequentially while input continues. Fixed snapshot storage lives in the activity;
there is no feedback queue, extra task or dynamic allocation. Hit targets stay enabled while refreshing;
live input validation enforces digit/decimal limits.

Host checks cover latest-state snapshots, consuming press bits once, independent expiry, timer wraparound,
and input arriving during an in-flight refresh.
On PaperMono, enter `12345` rapidly and repeated digits; verify the display catches up without replay.
Test `C` followed immediately by digits at the entry limit, operator/CE bursts, and Back during refresh.
Check every accepted input reaches the final result and the reader still opens safely. Display latency
and touch-controller sampling still require hardware validation.

The checked `scripts/patch_freeink_keyboard.py` build patch makes the pinned keyboard renderer honor
its existing per-key state field. This reuses the SDK styles without a custom keyboard renderer.
PaperMono check: tap several different keys rapidly and verify their shared highlight frame, then repeat
one key and verify every digit registers. Arrow navigation retains its own selection.

Each key has its own 250 ms feedback interval, starting when its new press highlight finishes displaying.
Repeating a key restarts only its interval; unrelated highlights retain their original expiry. Expired keys
request a refresh while still-active keys remain highlighted. Actual visible duration includes refresh latency. Navigation selection does not expire. On PaperMono,
tap a key and stop: its press highlight should clear without further input; also check rapid bursts.
