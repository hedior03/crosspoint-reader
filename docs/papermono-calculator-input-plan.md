# Calculator input: first baby step

Status: **implemented; owner confirmed successful upload and basic operation on PaperMono**.
Scope and arrangement agreed 2026-09-15. The full device checklist below has not been reported complete.
Parent: [PaperMono roadmap](papermono-roadmap.md).

## Goal

Home → Tools → Calculator. Press digits, see the sequence, press `C` to clear it.
This is an input screen, not yet an arithmetic calculator. The Tools corner and this screen are one increment.

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
- Final row: a wide `0` and a normal-width `C`, using the renderer's existing relative widths.
- Reuse the current keyboard's visual treatment and theme spacing. Use existing layout metrics and oriented
  viewable bounds for the screen; do not invent fixed pixel coordinates.
- Proposed readout: right-aligned existing UI text, initially blank. No cursor or direct text editing.

These composition choices were agreed before implementation. If the stock appearance does not suit,
discuss the specific difference before changing it.

## Behavior

- Each digit activation appends exactly one digit. Preserve leading zeros: `0`, `0`, `7` displays `007`.
- `C` clears the whole sequence to blank; clearing an empty sequence is harmless.
- A held key does not auto-repeat. Use existing navigation/gesture behavior, with no new gestures.
- Proposed bound: 12 digits. At the bound, disable digit entry using existing disabled key styling; `C` remains
  available. Verify all 12 digits fit with the reused font before accepting the layout.
- Input belongs to this screen session. Leaving and reopening starts blank; no SD writes or restoration.
- Use a fixed 13-byte character buffer plus a length for this proposed bound. No numeric conversion, dynamic
  input allocation, or arithmetic dependency is needed. Keep translations within the repository's `tr()` convention.

## Out of scope

Operations, equals, decimal point, negative sign, backspace, expression parsing, history, memory buttons,
copy/send, BLE, persistence, animation, custom key artwork, and a reusable calculator/app framework.
No hardware drivers or background tasks are needed for this increment.

## Verification when implemented

- One small runnable input-sequence check: append `007`, clear, clear again, reach the bound, reject an extra
  digit without changing the buffer, clear, append again. Include null termination in that check.
- PaperMono: tap every digit, tap quickly during refreshes, hold a key, tap near key boundaries, and confirm
  each deliberate activation produces exactly one digit. Verify clear and the full-length readout.
- Exercise Back → Tools, Home, sleep/wake, and reopen; then reopen a book and verify reading position and input.
- Check the reused layout and touch targets in the reader's supported orientations and the active theme.
  Do not claim the design matches from a host render alone.
- Build `papermono` once after the final code edit, then perform the device checks. No build/flash is needed
  for this planning document, and none has been performed for it.

## Implementation notes

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
