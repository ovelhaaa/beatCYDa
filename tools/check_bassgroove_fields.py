from pathlib import Path
import re

try:
    Import("env")
    project_dir = Path(env["PROJECT_DIR"])
except NameError:
    project_dir = Path(__file__).resolve().parents[1]


def _count_field(text: str, field: str) -> int:
    pattern = rf"^\s*float\s+{re.escape(field)}\s*;"
    return len(re.findall(pattern, text, flags=re.MULTILINE))


def _assert_unique_field_declarations() -> None:
    header = project_dir / "src" / "audio" / "BassGroove.h"
    text = header.read_text(encoding="utf-8")

    for field in ("swing", "ghostProb", "accentProb"):
        count = _count_field(text, field)
        if count != 1:
            raise RuntimeError(
                f"{header}: expected exactly 1 declaration for '{field}', found {count}"
            )


_assert_unique_field_declarations()
