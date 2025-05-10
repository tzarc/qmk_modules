#!/usr/bin/env python
# Copyright 2025 Nick Brassel (@tzarc)
# SPDX-License-Identifier: GPL-2.0-or-later

import json
import os
import subprocess
import sys
import struct
from pathlib import Path
from milc.questions import yesno

qmk_firmware_path = os.environ.get("QMK_FIRMWARE_DIR", None)
if qmk_firmware_path is None:
    raise FileNotFoundError("QMK_FIRMWARE_DIR environment variable not set. Please set it to the path of your QMK firmware directory.")
qmk_firmware_path = Path(qmk_firmware_path)
qmk_cli_path = qmk_firmware_path / "lib/python"

sys.path.append(str(qmk_cli_path))

orig_cwd = os.getcwd()
os.environ["ORIG_CWD"] = str(orig_cwd)
os.chdir(qmk_firmware_path)
os.environ["QMK_HOME"] = str(qmk_firmware_path)

import milc

milc.cli.milc_options(name="qmk")

import qmk_cli.subcommands  # noqa: F401
import qmk.cli  # noqa: F401
from qmk.keycodes import load_spec

try:
    import jinja2
except ImportError:
    if yesno("jinja2 is not installed, do you wish to install it?"):
        subprocess.check_call(["python", "-m", "pip", "install", "jinja2"])
        import jinja2
    else:
        raise

j2_env = jinja2.Environment(
    loader=jinja2.FileSystemLoader(Path(os.path.realpath(__file__)).parent),
    autoescape=jinja2.select_autoescape(),
)

j2_template = j2_env.get_template("keycode_lookup.c.j2")

keycode_data = load_spec("latest")

all_keycodes = set()
for value_txt, tbl in keycode_data["keycodes"].items():
    value_num = int(value_txt, base=16)
    all_keycodes.add((value_num, tbl["key"]))
    if "aliases" in tbl:
        for alias in tbl["aliases"]:
            all_keycodes.add((value_num, alias))

keycode_name_bytes = b''
keycode_offsets = []
for value, name in sorted(all_keycodes, key=lambda x: x[1]):
    new_offset = len(keycode_name_bytes)
    keycode_offsets.append((new_offset, value))
    keycode_name_bytes += name.encode("utf-8")

keycode_offset_bytes = b''
for offset, value in keycode_offsets:
    keycode_offset_bytes += struct.pack(">H", offset)
    keycode_offset_bytes += struct.pack(">H", value)

print(
    j2_template.render(
        all_keycodes=list(sorted(all_keycodes, key=lambda x: x[1])),
        keycode_offsets=keycode_offsets,
        keycode_offset_bytes=keycode_offset_bytes,
        keycode_name_bytes=keycode_name_bytes,
    )
)
