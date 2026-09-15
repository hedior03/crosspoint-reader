"""Honor KeyboardKey.state in the pinned SDK, matching its keyGrid component."""
from pathlib import Path

Import("env")

header = Path(env["PROJECT_DIR"]) / "freeink-sdk/libs/ui/FreeInkUI/include/components/keyboard/keyboard.h"
source = header.read_text()
old = """auto drawKey = [&](Rect keyRect, const KeyboardKey& key, int16_t selectedIndex) {
    State state = StateNormal;"""
new = old.replace("StateNormal", "key.state")
if source.count(old) == 1:
    header.write_text(source.replace(old, new))
elif source.count(new) != 1:
    raise RuntimeError("FreeInk keyboard changed: review the per-key state patch")
