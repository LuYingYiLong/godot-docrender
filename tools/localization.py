#!/usr/bin/env python3
"""Extract and check the Simplified Chinese editor catalogs for Godot DocRender."""

import argparse
from collections import defaultdict
import io
import json
from pathlib import Path
import re
import textwrap
import xml.etree.ElementTree as ET

from babel.messages.catalog import Catalog
from babel.messages.pofile import read_po, write_po

ROOT = Path(__file__).resolve().parents[1]
CATALOGS = ROOT / "addons/docrender/translations"
LOCALE = "zh_Hans"
EDITOR_TEXT = re.compile(r'\btr\(\s*("(?:\\.|[^"\\])*")\s*,\s*("Godot DocRender")\s*\)')
PROPERTY = re.compile(r'\bADD_PROPERTYI?\(PropertyInfo\(Variant::\w+,\s*"([^"]+)"')
PROPERTY_GROUP = re.compile(r'\bADD_(?:SUB)?GROUP\("([^"]+)"')
STOP_WORDS = set("a an and as at by for in not of on or over per the then to with".split())
DOC_TAGS = {"brief_description", "description", "member", "constant", "theme_item"}
DOC_MARKUP = re.compile(r'\[code\].*?\[/code\]|\[/?(?:member|method|signal|constant|theme_item|enum|param|b|i)(?: [^\]]+)?\]|\[[A-Z]\w*(?:\.\w+)?\]', re.DOTALL)

# Match PO escapes in pairs, so a literal backslash is distinct from a newline escape.
PO_ESCAPE = re.compile(r'(\\\\)(?=[abfnrtv])|\\.')


def format_godot_po(text):
    # Godot 4.7.2 decodes each quoted fragment separately. End the fragment
    # after an escaped backslash so commands like \frac stay literal.
    def protect_escape(match):
        return match[0] + '"\n"' if match[1] else match[0]

    return "\n".join(
        PO_ESCAPE.sub(protect_escape, line)
        if line.startswith(('"', 'msgid "', 'msgstr "')) else line
        for line in text.split("\n")
    )


def inspector_label(name):
    words = name.rsplit("/", 1)[-1].split("_")
    return " ".join(
        word if 0 < index < len(words) - 1 and word in STOP_WORDS else word.capitalize()
        for index, word in enumerate(words)
    )


def collect_sources():
    messages = {name: defaultdict(set) for name in ("editor", "properties", "documentation")}

    for path in sorted((ROOT / "src/editor").glob("*.cpp")):
        text = path.read_text(encoding="utf-8")
        for match in EDITOR_TEXT.finditer(text):
            key = (json.loads(match[1]), json.loads(match[2]))
            line = text.count("\n", 0, match.start()) + 1
            messages["editor"][key].add(f"{path.relative_to(ROOT).as_posix()}:{line}")

    for path in sorted((ROOT / "src/nodes").glob("*.cpp")):
        text = path.read_text(encoding="utf-8")
        for pattern in (PROPERTY, PROPERTY_GROUP):
            for match in pattern.finditer(text):
                label = inspector_label(match[1]) if pattern is PROPERTY else match[1]
                messages["properties"][label, None].add(path.relative_to(ROOT).as_posix())

    for path in sorted((ROOT / "doc_classes").glob("*.xml")):
        root = ET.parse(path).getroot()
        for element in root.iter():
            if element.tag not in DOC_TAGS:
                continue
            # Godot looks up the full XML description after removing indentation.
            source = textwrap.dedent(element.text or "").strip()
            if source:
                messages["documentation"][source, None].add(path.relative_to(ROOT).as_posix())

    return messages


def template(messages):
    catalog = Catalog(project="Godot DocRender")
    for (source, context), locations in sorted(messages.items()):
        catalog.add(source, context=context, auto_comments=sorted(locations))
    return catalog


def load_catalog(path):
    if path.exists():
        with path.open("rb") as stream:
            return read_po(stream, locale=LOCALE, abort_invalid=True)
    return Catalog(locale=LOCALE, project="Godot DocRender", charset="utf-8", fuzzy=False)


def check_catalog(path, catalog, messages):
    keys = {(message.id, message.context) for message in catalog if message.id}
    problems = [f"{path.name}: missing {source!r}" for source, _ in sorted(messages.keys() - keys)]
    problems += [f"{path.name}: obsolete {source!r}" for source, _ in sorted(keys - messages.keys())]
    for message in catalog:
        if not message.id:
            continue
        if not message.string or message.fuzzy:
            problems.append(f"{path.name}: needs translation or review: {message.id!r}")
            continue
        if sorted(DOC_MARKUP.findall(message.id)) != sorted(DOC_MARKUP.findall(message.string)):
            problems.append(f"{path.name}: changed Godot markup or code: {message.id!r}")
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("update", "check"))
    args = parser.parse_args()
    problems = []
    for name, messages in collect_sources().items():
        path = CATALOGS / f"{name}.{LOCALE}.po"
        catalog = load_catalog(path)
        if args.action == "update":
            catalog.update(template(messages))
            output = io.BytesIO()
            write_po(output, catalog, width=100, sort_output=True, include_previous=True, ignore_obsolete=True)
            text = output.getvalue().decode("utf-8").rstrip() + "\n"
            path.write_text(format_godot_po(text), encoding="utf-8", newline="\n")
        elif path.is_file():
            text = path.read_text(encoding="utf-8")
            if text != format_godot_po(text):
                problems.append(f"{path.name}: unsafe PO escapes for Godot; run update")
        problems.extend(check_catalog(path, catalog, messages))
        print(f"{path.name}: {len(messages)} source entries")
    for problem in problems:
        print(problem)
    return int(bool(problems))


if __name__ == "__main__":
    raise SystemExit(main())
