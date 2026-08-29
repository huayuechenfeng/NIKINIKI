#!/usr/bin/env python3
"""Build the small OFL font fallback that is linked into the NIKINIKI EXE.

The complete OFL font is still shipped as a SIS data file.  This
fallback contains GB2312 plus the characters used by this repository, so a
failed installed-drive lookup cannot leave the NanoVG UI completely blank.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont


TEXT_SUFFIXES = {
    ".c", ".cc", ".cpp", ".h", ".hpp", ".md", ".pkg", ".pro", ".qrc",
    ".txt", ".xml",
}


def gb2312_codepoints() -> set[int]:
    result: set[int] = set()
    for lead in range(0xA1, 0xF8):
        for trail in range(0xA1, 0xFF):
            try:
                text = bytes((lead, trail)).decode("gb2312")
            except UnicodeDecodeError:
                continue
            result.update(ord(character) for character in text)
    return result


def repository_codepoints(root: Path) -> set[int]:
    result: set[int] = set()
    ignored = {".git", ".tmp", "library", "out", "upstream"}
    for path in root.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in TEXT_SUFFIXES:
            continue
        if any(part.lower() in ignored for part in path.relative_to(root).parts):
            continue
        try:
            result.update(ord(character) for character in path.read_text("utf-8"))
        except (OSError, UnicodeDecodeError):
            continue
    return result


def add_range(target: set[int], first: int, last: int) -> None:
    target.update(range(first, last + 1))


def rename_modified_font(font: TTFont) -> None:
    """OFL 1.1 requires a modified build not to retain reserved name Source."""
    names = font["name"]
    replacements = {
        1: "NIKINIKI CJK",
        2: "Regular",
        3: "NIKINIKI CJK Regular 0.9.0",
        4: "NIKINIKI CJK Regular",
        5: "Version 0.9.0",
        6: "NIKINIKICJK-Regular",
        16: "NIKINIKI CJK",
        17: "Regular",
    }
    records = list(names.names)
    for name_id, value in replacements.items():
        matching = [record for record in records if record.nameID == name_id]
        if matching:
            for record in matching:
                names.setName(
                    value, name_id, record.platformID, record.platEncID,
                    record.langID,
                )
        else:
            names.setName(value, name_id, 3, 1, 0x0409)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--root", type=Path, default=Path(__file__).parents[1])
    args = parser.parse_args()

    codepoints = gb2312_codepoints() | repository_codepoints(args.root.resolve())
    for first, last in (
        (0x0020, 0x024F),  # Latin, IPA and punctuation used by API content
        (0x0370, 0x052F),  # Greek and Cyrillic account/video names
        (0x2000, 0x206F),  # general punctuation
        (0x20A0, 0x20CF),  # currency symbols
        (0x2100, 0x22FF),  # letterlike, arrows and maths
        (0x2460, 0x27BF),  # enclosed characters, shapes and dingbats
        (0x2E80, 0x303F),  # CJK radicals, punctuation and kana marks
        (0x3040, 0x30FF),  # hiragana and katakana
        (0x3100, 0x312F),  # bopomofo
        (0x31A0, 0x31BF),
        (0x31F0, 0x31FF),
        (0xFE10, 0xFE6F),  # vertical/small CJK forms
        (0xFF00, 0xFFEF),  # fullwidth forms
    ):
        add_range(codepoints, first, last)
    codepoints.add(0xFFFD)

    options = subset.Options()
    options.hinting = False
    options.recalc_timestamp = False
    options.layout_features = ["*"]
    options.name_IDs = [0, 1, 2, 3, 4, 5, 6, 16, 17]
    options.name_languages = ["*"]
    options.drop_tables += ["DSIG"]

    font = TTFont(str(args.input), recalcTimestamp=False)
    available = set(font.getBestCmap())
    selected = codepoints & available
    subsetter = subset.Subsetter(options=options)
    subsetter.populate(unicodes=selected)
    subsetter.subset(font)
    rename_modified_font(font)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    font.save(str(args.output), reorderTables=True)
    print(
        f"wrote {args.output}: {args.output.stat().st_size} bytes, "
        f"{len(selected)} mapped codepoints"
    )


if __name__ == "__main__":
    main()
