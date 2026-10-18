#!/usr/bin/env python3
"""Generate and compile gettext catalogs for TechHaven-Server error codes."""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

PROJECT_NAME = "TechHaven-Server"
DOMAIN_PREFIX = "techhaven"
LANGUAGES = {"en": "en", "zh_CN": "zh"}
KEY_RE = re.compile(r"^[A-Z0-9_]+$")
PRINTF_RE = re.compile(r"%(?:[-+ #0]*\d*(?:\.\d+)?[diouxXeEfFgGaAcspn%])")


class CatalogError(Exception):
    pass


def po_escape(value):
    return (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )


def po_unescape(value):
    result = []
    index = 0
    escapes = {
        "n": "\n",
        "r": "\r",
        "t": "\t",
        '"': '"',
        "\\": "\\",
    }
    while index < len(value):
        char = value[index]
        if char == "\\" and index + 1 < len(value):
            nxt = value[index + 1]
            result.append(escapes.get(nxt, nxt))
            index += 2
            continue
        result.append(char)
        index += 1
    return "".join(result)


def load_errors(project_dir):
    path = project_dir / "errors.json"
    if not path.is_file():
        raise CatalogError(f"errors.json not found: {path}")

    try:
        with path.open("r", encoding="utf-8") as stream:
            root = json.load(stream)
    except json.JSONDecodeError as exc:
        raise CatalogError(f"failed to parse {path}: {exc}") from exc

    errors = root.get("errors")
    if not isinstance(errors, list):
        raise CatalogError("errors.json: 'errors' must be an array")

    seen_errno = {}
    seen_key = {}
    normalized = []
    for index, item in enumerate(errors):
        if not isinstance(item, dict):
            raise CatalogError(f"errors[{index}] must be an object")

        errno = item.get("errno")
        key = item.get("key")
        zh = item.get("zh")
        en = item.get("en")

        if not isinstance(errno, int):
            raise CatalogError(f"errors[{index}].errno must be an integer")
        if not isinstance(key, str) or not KEY_RE.fullmatch(key):
            raise CatalogError(f"errors[{index}].key is invalid: {key!r}")
        if not isinstance(zh, str) or not zh:
            raise CatalogError(f"errors[{index}].zh must be a non-empty string")
        if not isinstance(en, str) or not en:
            raise CatalogError(f"errors[{index}].en must be a non-empty string")

        if errno in seen_errno:
            raise CatalogError(f"duplicate errno {errno}: {key} and {seen_errno[errno]}")
        if key in seen_key:
            raise CatalogError(f"duplicate key {key}: errno {errno} and {seen_key[key]}")
        seen_errno[errno] = key
        seen_key[key] = errno

        normalized.append({"errno": errno, "key": key, "zh": zh, "en": en})

    if not normalized:
        raise CatalogError("errors.json contains no error codes")

    return root.get("version", "unknown"), normalized


def header(language):
    version = os.environ.get("SOURCE_DATE_EPOCH")
    try:
        timestamp = (
            datetime.fromtimestamp(int(version), tz=timezone.utc)
            if version is not None
            else datetime.now(tz=timezone.utc)
        )
    except (TypeError, ValueError, OSError):
        timestamp = datetime.now(tz=timezone.utc)
    date = timestamp.strftime("%Y-%m-%d %H:%M%z")
    plural = (
        "nplurals=2; plural=(n != 1);"
        if language == "en"
        else "nplurals=1; plural=0;"
    )
    return [
        'msgid ""',
        'msgstr ""',
        f'"Project-Id-Version: {PROJECT_NAME}\\n"',
        '"Report-Msgid-Bugs-To: \\n"',
        f'"POT-Creation-Date: {date}\\n"',
        '"PO-Revision-Date: \\n"',
        '"Last-Translator: \\n"',
        '"Language-Team: \\n"',
        f'"Language: {language}\\n"',
        '"MIME-Version: 1.0\\n"',
        '"Content-Type: text/plain; charset=UTF-8\\n"',
        '"Content-Transfer-Encoding: 8bit\\n"',
        f'"Plural-Forms: {plural}\\n"',
        '"X-Generator: scripts/i18n.py\\n"',
    ]


def render_pot(version, entries):
    lines = header("")
    lines[2] = f'"Project-Id-Version: {PROJECT_NAME} {version}\\n"'
    for entry in entries:
        lines.append("")
        lines.append(f"#. errno: {entry['errno']}")
        lines.append(f'msgid "{po_escape(entry["key"])}"')
        lines.append('msgstr ""')
    return "\n".join(lines) + "\n"


def render_po(version, language, entries):
    source_field = LANGUAGES[language]
    lines = header(language)
    lines[2] = f'"Project-Id-Version: {PROJECT_NAME} {version}\\n"'
    for entry in entries:
        lines.append("")
        lines.append(f"#. errno: {entry['errno']}")
        lines.append(f'msgid "{po_escape(entry["key"])}"')
        lines.append(f'msgstr "{po_escape(entry[source_field])}"')
    return "\n".join(lines) + "\n"


def parse_po(path):
    if not path.is_file():
        raise CatalogError(f"catalog not found: {path}")

    entries = {}
    current = {}
    field = None
    with path.open("r", encoding="utf-8") as stream:
        for raw_line in stream:
            line = raw_line.rstrip("\n")
            if line.startswith("#"):
                continue
            if line.startswith("msgid "):
                if current.get("msgid") is not None:
                    entries[current["msgid"]] = current.get("msgstr", "")
                current = {"msgid": po_unescape(line[6:].strip()[1:-1]), "msgstr": ""}
                field = "msgid"
                continue
            if line.startswith("msgstr "):
                current["msgstr"] = po_unescape(line[7:].strip()[1:-1])
                field = "msgstr"
                continue
            if line.startswith('"') and field:
                value = po_unescape(line.strip()[1:-1])
                current[field] += value
                continue

    if current.get("msgid") is not None:
        entries[current["msgid"]] = current.get("msgstr", "")

    entries.pop("", None)
    return entries


def placeholder_types(value):
    return sorted(
        item[-1]
        for item in PRINTF_RE.findall(value)
        if item != "%%"
    )


def sync(project_dir):
    version, entries = load_errors(project_dir)
    locale_dir = project_dir / "locale"
    po_dir = locale_dir / "po"
    po_dir.mkdir(parents=True, exist_ok=True)

    (locale_dir / f"{DOMAIN_PREFIX}.pot").write_text(
        render_pot(version, entries), encoding="utf-8", newline="\n"
    )

    for language in LANGUAGES:
        path = po_dir / f"{language}.po"
        path.write_text(
            render_po(version, language, entries), encoding="utf-8", newline="\n"
        )
        print(f"generated {path.relative_to(project_dir)}")

    print(f"generated {(locale_dir / f'{DOMAIN_PREFIX}.pot').relative_to(project_dir)}")
    print(f"{len(entries)} error codes")


def check(project_dir):
    version, entries = load_errors(project_dir)
    locale_dir = project_dir / "locale"
    po_dir = locale_dir / "po"
    pot_path = locale_dir / f"{DOMAIN_PREFIX}.pot"

    if not pot_path.is_file():
        raise CatalogError(f"catalog not found: {pot_path}")

    pot_entries = parse_po(pot_path)
    expected_keys = {entry["key"] for entry in entries}
    missing_pot = expected_keys - set(pot_entries)
    extra_pot = set(pot_entries) - expected_keys
    if missing_pot:
        raise CatalogError(f"POT is missing keys: {', '.join(sorted(missing_pot))}")
    if extra_pot:
        print(f"warning: POT has unknown keys: {', '.join(sorted(extra_pot))}")

    for language in LANGUAGES:
        path = po_dir / f"{language}.po"
        catalog = parse_po(path)
        missing = expected_keys - set(catalog)
        if missing:
            raise CatalogError(
                f"{path.name} is missing keys: {', '.join(sorted(missing))}"
            )

        for entry in entries:
            translated = catalog[entry["key"]]
            if not translated:
                raise CatalogError(
                    f"{path.name}: empty translation for {entry['key']}"
                )
            if placeholder_types(entry["key"]) != placeholder_types(translated):
                raise CatalogError(
                    f"{path.name}: placeholder mismatch for {entry['key']}"
                )

        print(f"checked {path.relative_to(project_dir)} ({len(catalog)} entries)")

    print(f"checked {pot_path.relative_to(project_dir)} ({len(pot_entries)} entries)")
    print(f"version {version}")


def build(project_dir, msgfmt):
    if not msgfmt:
        raise CatalogError(
            "msgfmt not found; install gettext or pass --msgfmt <path>"
        )

    po_dir = project_dir / "locale" / "po"
    output_dir = project_dir / "locale" / "C" / "LC_MESSAGES"
    output_dir.mkdir(parents=True, exist_ok=True)

    for language in LANGUAGES:
        po_path = po_dir / f"{language}.po"
        if not po_path.is_file():
            raise CatalogError(f"catalog not found: {po_path}")
        mo_path = output_dir / f"{DOMAIN_PREFIX}_{language}.mo"
        command = [
            msgfmt,
            "--check",
            "--check-format",
            "--statistics",
            "-o",
            str(mo_path),
            str(po_path),
        ]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode != 0:
            message = (result.stderr or result.stdout).strip()
            raise CatalogError(f"msgfmt failed for {po_path.name}: {message}")
        print(f"compiled {mo_path.relative_to(project_dir)}")


def main():
    default_project = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-dir", type=Path, default=default_project)
    parser.add_argument("--msgfmt", default=None)
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("sync", help="regenerate locale/techhaven.pot and PO files")
    subparsers.add_parser("check", help="validate errors.json and catalogs")
    subparsers.add_parser("build", help="compile PO files into MO files")
    args = parser.parse_args()

    project_dir = args.project_dir.resolve()
    msgfmt = args.msgfmt or shutil.which("msgfmt")

    try:
        if args.command == "sync":
            sync(project_dir)
        elif args.command == "check":
            check(project_dir)
        else:
            build(project_dir, msgfmt)
    except CatalogError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
