#!/usr/bin/env python3
"""Download the test ROM suites the acceptance runner expects.

Nothing here is vendored into the repo: the ROMs are freely redistributable but
large and not ours, so they live in tests/roms/ which is gitignored.
"""

import argparse
import hashlib
import io
import json
import lzma
import os
import re
import shutil
import sys
import tarfile
import urllib.error
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROMS = os.path.join(ROOT, "tests", "roms")
MANIFEST = os.path.join(ROMS, "manifest.json")

USER_AGENT = "my_gb-fetch-test-roms"


def get(url):
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=60) as resp:
        return resp.read()


def latest_release(repo):
    data = json.loads(get("https://api.github.com/repos/%s/releases/latest" % repo))
    return data


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def write(dest, data):
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    with open(dest, "wb") as f:
        f.write(data)


def open_archive(blob, name_hint):
    if name_hint.endswith(".zip"):
        return "zip", zipfile.ZipFile(io.BytesIO(blob))
    if name_hint.endswith(".tar.xz"):
        return "tar", tarfile.open(fileobj=io.BytesIO(lzma.decompress(blob)))
    return "tar", tarfile.open(fileobj=io.BytesIO(blob), mode="r:gz")


def extract(blob, name_hint, dest, keep, strip_to=None):
    """Extract members whose name passes keep() into dest, flattening the
    leading directory produced by the archive."""
    kind, archive = open_archive(blob, name_hint)
    count = 0
    names = archive.namelist() if kind == "zip" else archive.getnames()
    for name in names:
        if name.endswith("/") or not keep(name):
            continue
        parts = name.split("/")
        if strip_to is not None and strip_to in parts:
            rel = "/".join(parts[parts.index(strip_to):])
        else:
            rel = "/".join(parts[1:]) if len(parts) > 1 else parts[0]
        if not rel or ".." in rel or rel.startswith("/"):
            continue
        if kind == "zip":
            data = archive.read(name)
        else:
            member = archive.extractfile(name)
            if member is None:
                continue
            data = member.read()
        write(os.path.join(dest, rel), data)
        count += 1
    archive.close()
    return count


def fetch_blargg():
    dest = os.path.join(ROMS, "blargg")
    blob = get("https://codeload.github.com/retrio/gb-test-roms/tar.gz/refs/heads/master")
    n = extract(blob, "master.tar.gz", dest, lambda p: p.endswith(".gb"))
    return "blargg: %d roms" % n


def fetch_dmg_acid2():
    dest = os.path.join(ROMS, "dmg-acid2")
    rel = latest_release("mattcurrie/dmg-acid2")
    asset = next((a for a in rel["assets"] if a["name"] == "dmg-acid2.gb"), None)
    if asset is None:
        raise RuntimeError("dmg-acid2.gb not in the latest release")
    write(os.path.join(dest, "dmg-acid2.gb"), get(asset["browser_download_url"]))
    return "dmg-acid2: 1 rom (%s)" % rel["tag_name"]


def fetch_mooneye():
    # The suite has no GitHub releases; Gekkio publishes prebuilt ROMs here.
    dest = os.path.join(ROMS, "mooneye")
    index = "https://gekkio.fi/files/mooneye-test-suite/"
    builds = sorted(set(re.findall(r"mts-\d{8}-\d{4}-[0-9a-f]+", get(index).decode("utf-8"))))
    if not builds:
        raise RuntimeError("no mts builds listed at %s" % index)
    build = builds[-1]
    blob = get("%s%s/%s.zip" % (index, build, build))
    n = extract(blob, build + ".zip", dest, lambda p: p.endswith(".gb"))
    return "mooneye: %d roms (%s)" % (n, build)


def fetch_sst():
    dest = os.path.join(ROMS, "sst")
    blob = get("https://codeload.github.com/SingleStepTests/sm83/tar.gz/refs/heads/main")
    n = extract(
        blob,
        "main.tar.gz",
        dest,
        lambda p: "/v1/" in p and p.endswith(".json"),
        strip_to="v1",
    )
    return "sst: %d opcode vector files" % n


SOURCES = {
    "blargg": (fetch_blargg, os.path.join(ROMS, "blargg")),
    "dmg-acid2": (fetch_dmg_acid2, os.path.join(ROMS, "dmg-acid2")),
    "mooneye": (fetch_mooneye, os.path.join(ROMS, "mooneye")),
    "sst": (fetch_sst, os.path.join(ROMS, "sst")),
}


def load_manifest():
    if not os.path.exists(MANIFEST):
        return {}
    try:
        with open(MANIFEST) as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def build_manifest():
    out = {}
    for base, _, files in os.walk(ROMS):
        for name in sorted(files):
            path = os.path.join(base, name)
            if path == MANIFEST:
                continue
            out[os.path.relpath(path, ROMS)] = sha256(path)
    return out


def verify(manifest, prefix):
    """Return True when everything recorded under prefix is present and intact."""
    entries = [k for k in manifest if k.startswith(prefix + os.sep) or k.startswith(prefix + "/")]
    if not entries:
        return False
    for rel in entries:
        path = os.path.join(ROMS, rel)
        if not os.path.exists(path) or sha256(path) != manifest[rel]:
            return False
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--only", choices=sorted(SOURCES), help="fetch a single source")
    ap.add_argument("--force", action="store_true", help="re-download even if present")
    args = ap.parse_args()

    os.makedirs(ROMS, exist_ok=True)
    manifest = load_manifest()
    wanted = [args.only] if args.only else sorted(SOURCES)

    failed = False
    for name in wanted:
        fetch, dest = SOURCES[name]
        if not args.force and verify(manifest, name):
            print("%s: up to date" % name)
            continue
        if args.force and os.path.isdir(dest):
            shutil.rmtree(dest)
        try:
            print(fetch())
        except (urllib.error.URLError, urllib.error.HTTPError, OSError, RuntimeError) as exc:
            print("%s: failed (%s)" % (name, exc), file=sys.stderr)
            failed = True

    with open(MANIFEST, "w") as f:
        json.dump(build_manifest(), f, indent=1, sort_keys=True)
        f.write("\n")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
