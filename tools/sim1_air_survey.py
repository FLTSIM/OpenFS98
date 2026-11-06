#!/usr/bin/env python3
"""Inventory reference AIR chunks and installed byte matches; no decoding.

Only hashes, offsets, ids and lengths are emitted into ignored build output.
The ordered per-file manifest is input evidence for the pending SIM1 decoder.
"""
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]


def chunks(content):
    result = []
    offset = 0
    while offset < len(content):
        if len(content) - offset < 8:
            raise ValueError(f"truncated AIR chunk header at {offset:#x}")
        tag, size = struct.unpack_from("<II", content, offset)
        end = offset + 8 + size
        if end > len(content):
            raise ValueError(f"AIR chunk {tag:#x} at {offset:#x} exceeds file")
        result.append({"offset": offset, "tag": tag, "size": size})
        offset = end
    return result


def main():
    installed = defaultdict(list)
    for path in sorted((ROOT / "game/Aircraft").rglob("*")):
        if path.is_file() and path.suffix.lower() == ".air":
            installed[hashlib.sha256(path.read_bytes()).hexdigest()].append(str(path.relative_to(ROOT)))
    files = []
    kinds = defaultdict(lambda: {"count": 0, "sizes": set(), "files": set()})
    for path in sorted((ROOT / "cd/fs98/Aircraft").rglob("*")):
        if not path.is_file() or path.suffix.lower() != ".air":
            continue
        content = path.read_bytes()
        digest = hashlib.sha256(content).hexdigest()
        records = chunks(content)
        relative = str(path.relative_to(ROOT))
        files.append({"source": relative, "sha256": digest, "size": len(content),
                      "installed_matches": installed[digest], "chunks": records})
        for record in records:
            row = kinds[record["tag"]]
            row["count"] += 1
            row["sizes"].add(record["size"])
            row["files"].add(relative)
    if not files:
        raise SystemExit("No reference CD aircraft AIR files found")
    manifest = {"files": files, "types": [
        {"tag": tag, "count": row["count"], "sizes": sorted(row["sizes"]), "files": sorted(row["files"])}
        for tag, row in sorted(kinds.items())]}
    output = ROOT / "bin/sim1-diff/aircraft-survey.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2) + "\n")
    total = sum(len(row["chunks"]) for row in files)
    matching = sum(bool(row["installed_matches"]) for row in files)
    print(f"{len(files)} CD AIR files, {total} chunks, {len(kinds)} tags; {matching} installed byte matches")
    print(output.relative_to(ROOT))


if __name__ == "__main__":
    main()
