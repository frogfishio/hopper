#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Frogfish
# SPDX-License-Identifier: Apache-2.0
# Author: Alexander Croft <alex@frogfish.io>
"""
Minimal JSON -> Hopper catalog loader (Python).
Demonstrates parsing a catalog JSON file and initializing a Hopper context via ctypes.
"""
import json
import ctypes
from ctypes import c_int32, c_uint32, c_uint16, c_uint8, c_size_t, POINTER
import os
import sys

lib = ctypes.CDLL(os.environ.get("HOPPER_LIB", "libhopper.so"))

# Structs mirroring hopper.h
class HopperResultRef(ctypes.Structure):
    _fields_ = [("ok", c_int32), ("err", c_int32), ("ref", c_int32)]

class HopperBytes(ctypes.Structure):
    _fields_ = [("ptr", POINTER(c_uint8)), ("len", c_uint32)]

class HopperBytesMut(ctypes.Structure):
    _fields_ = [("ptr", POINTER(c_uint8)), ("len", c_uint32)]

class HopperPic(ctypes.Structure):
    _fields_ = [
        ("digits", c_uint16),
        ("scale", c_uint16),
        ("is_signed", c_uint8),
        ("usage", c_uint8),
        ("mask_ascii", ctypes.c_char_p),
        ("mask_len", c_uint16),
    ]

class HopperField(ctypes.Structure):
    _fields_ = [
        ("name_ascii", ctypes.c_char_p),
        ("name_len", c_uint16),
        ("offset", c_uint32),
        ("size", c_uint32),
        ("kind", c_uint32),
        ("pad_byte", c_uint8),
        ("pic", HopperPic),
        ("redefines_index", c_int32),
    ]

class HopperLayout(ctypes.Structure):
    _fields_ = [
        ("name_ascii", ctypes.c_char_p),
        ("name_len", c_uint16),
        ("record_bytes", c_uint32),
        ("layout_id", c_uint32),
        ("fields", POINTER(HopperField)),
        ("field_count", c_uint32),
    ]

class HopperCatalog(ctypes.Structure):
    _fields_ = [
        ("abi_version", c_uint32),
        ("layouts", POINTER(HopperLayout)),
        ("layout_count", c_uint32),
    ]

class HopperConfig(ctypes.Structure):
    _fields_ = [
        ("abi_version", c_uint32),
        ("arena_mem", ctypes.c_void_p),
        ("arena_bytes", c_uint32),
        ("ref_mem", ctypes.c_void_p),
        ("ref_count", c_uint32),
        ("catalog", POINTER(HopperCatalog)),
    ]

# Function signatures
lib.hopper_sizeof.restype = c_size_t
lib.hopper_ref_entry_sizeof.restype = c_size_t
lib.hopper_version.restype = c_uint32
lib.hopper_init.restype = c_int32
lib.hopper_record.restype = HopperResultRef
lib.hopper_field_set_bytes.restype = c_int32
lib.hopper_field_get_bytes.restype = c_int32

USAGE_MAP = {
    "display": 1,
    "comp": 2,
    "comp3": 3,
}
KIND_MAP = {
    "bytes": 1,
    "num_i32": 2,
}

def load_catalog(path: str) -> HopperCatalog:
    data = json.loads(open(path, "r", encoding="utf-8").read())
    layouts = []
    for l in data["layouts"]:
        fields = []
        for f in l["fields"]:
            pic_data = f.get("pic")
            if pic_data:
                mask = pic_data.get("mask")
                pic = HopperPic(
                    digits=pic_data["digits"],
                    scale=pic_data.get("scale", 0),
                    is_signed=1 if pic_data.get("is_signed") else 0,
                    usage=USAGE_MAP[pic_data["usage"]],
                    mask_ascii=mask.encode() if mask else None,
                    mask_len=len(mask) if mask else 0,
                )
            else:
                pic = HopperPic()
            fields.append(
                HopperField(
                    name_ascii=f["name"].encode(),
                    name_len=len(f["name"]),
                    offset=f["offset"],
                    size=f["size"],
                    kind=KIND_MAP[f["kind"]],
                    pad_byte=f.get("pad_byte", 32),
                    pic=pic,
                    redefines_index=f.get("redefines_index", -1),
                )
            )
        field_array = (HopperField * len(fields))(*fields)
        layouts.append(
            HopperLayout(
                name_ascii=l["name"].encode(),
                name_len=len(l["name"]),
                record_bytes=l["record_bytes"],
                layout_id=l["layout_id"],
                fields=field_array,
                field_count=len(fields),
            )
        )
    layout_array = (HopperLayout * len(layouts))(*layouts)
    return HopperCatalog(
        abi_version=data["abi_version"],
        layouts=layout_array,
        layout_count=len(layouts),
    )

def main():
    if len(sys.argv) < 2:
        print("Usage: load_catalog.py path/to/catalog.json", file=sys.stderr)
        sys.exit(1)
    catalog = load_catalog(sys.argv[1])
    ctx_size = lib.hopper_sizeof()
    ref_entry = lib.hopper_ref_entry_sizeof()
    arena = (c_uint8 * 128)()
    refs = (c_uint8 * (ref_entry * 8))()
    ctx = (c_uint8 * ctx_size)()
    cfg = HopperConfig(
        abi_version=lib.hopper_version(),
        arena_mem=ctypes.cast(arena, ctypes.c_void_p),
        arena_bytes=len(arena),
        ref_mem=ctypes.cast(refs, ctypes.c_void_p),
        ref_count=8,
        catalog=ctypes.pointer(catalog),
    )
    hopper_ptr = ctypes.c_void_p()
    err = lib.hopper_init(ctypes.cast(ctx, ctypes.c_void_p), ctypes.byref(cfg), ctypes.byref(hopper_ptr))
    if err != 0:
        raise SystemExit(f"init failed: {err}")
    res = lib.hopper_record(ctypes.cast(hopper_ptr, ctypes.c_void_p), 1)
    print("alloc ok:", res.ok == 1, "err:", res.err)

if __name__ == "__main__":
    main()
