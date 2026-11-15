#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Put the web doctor together as a folder a static web server can serve.

  python3 web/doctor/assemble.py OUT_DIR PACK.zip [PACK.zip ...]

Each PACK is a cure pack made by `tools/c5doctor/c5doctor.py pack`. It is opened
and checked the way `cure` checks it, and its pieces are laid out under
OUT_DIR/packs/<board>/ with a plan.json the page reads: what goes where, how
long it is, and the SHA-256 and MD5 it must have.

A build carries the folder paths of the computer it was made on. This refuses a
pack whose files contain the path of this user's home folder.
"""
import hashlib, json, os, shutil, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "c5doctor"))
import c5doctor

SITE_FILES = ("index.html", "app.mjs", "doctor.mjs", "rom.mjs")
NAMES = {c5doctor.BOOTLOADER_AT: "bootloader.bin", c5doctor.TABLE_AT: "partitions.bin"}


def assemble(out, packs):
    os.makedirs(out, exist_ok=True)
    for name in SITE_FILES:
        shutil.copyfile(os.path.join(HERE, name), os.path.join(out, name))
    shutil.copytree(os.path.join(HERE, "vendor"), os.path.join(out, "vendor"), dirs_exist_ok=True)
    open(os.path.join(out, ".nojekyll"), "w").close()
    home = os.path.expanduser("~").encode()
    for path in packs:
        manifest, parts = c5doctor.open_pack(path)
        plan = c5doctor.cure_plan(parts)
        if len(plan) != 4:
            sys.exit("%s: its partition table has no start-up choice (otadata); the page expects one" % path)
        folder = os.path.join(out, "packs", manifest["board"])
        os.makedirs(folder, exist_ok=True)
        listed = []
        for index, (address, data) in enumerate(plan):
            name = NAMES.get(address, "otadata.bin" if index == 2 else "firmware.bin")
            if home in data:
                sys.exit("%s: %s contains this computer's home folder path; rebuild with -ffile-prefix-map" % (path, name))
            with open(os.path.join(folder, name), "wb") as piece:
                piece.write(data)
            listed.append({"file": name, "offset": address, "bytes": len(data),
                           "sha256": hashlib.sha256(data).hexdigest(), "md5": hashlib.md5(data).hexdigest()})
        with open(os.path.join(folder, "plan.json"), "w") as said:
            json.dump({"chip": "esp32c5", "board": manifest["board"], "program": manifest.get("program", ""), "parts": listed}, said, indent=2)
            said.write("\n")
        print("%s: %s, %d bytes in 4 pieces" % (manifest["board"], manifest.get("program", ""), sum(p["bytes"] for p in listed)))


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    assemble(sys.argv[1], sys.argv[2:])
