# SPDX-FileCopyrightText: 2026 Frogfish
# SPDX-License-Identifier: Apache-2.0
# Author: Alexander Croft <alex@frogfish.io>

import ctypes
from ctypes import c_int32, c_uint32, c_uint16, c_uint8, c_size_t, POINTER, Structure
import os

# Adjust path if libhopper is not on the default loader path.
lib = ctypes.CDLL(os.environ.get("HOPPER_LIB", "libhopper.so"))

class HopperResultRef(Structure):
    _fields_ = [("ok", c_int32), ("err", c_int32), ("ref", c_int32)]

class HopperBytes(Structure):
    _fields_ = [("ptr", POINTER(c_uint8)), ("len", c_uint32)]

class HopperBytesMut(Structure):
    _fields_ = [("ptr", POINTER(c_uint8)), ("len", c_uint32)]

class HopperPic(Structure):
    _fields_ = [
        ("digits", c_uint32),
        ("scale", c_uint32),
        ("is_signed", c_uint8),
        ("usage", c_uint8),
        ("mask_ascii", ctypes.c_char_p),
        ("mask_len", c_uint32),
    ]

class HopperField(Structure):
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

class HopperLayout(Structure):
    _fields_ = [
        ("name_ascii", ctypes.c_char_p),
        ("name_len", c_uint16),
        ("record_bytes", c_uint32),
        ("layout_id", c_uint32),
        ("fields", POINTER(HopperField)),
        ("field_count", c_uint32),
    ]

class HopperCatalog(Structure):
    _fields_ = [
        ("abi_version", c_uint32),
        ("layouts", POINTER(HopperLayout)),
        ("layout_count", c_uint32),
    ]

class HopperConfig(Structure):
    _fields_ = [
        ("abi_version", c_uint32),
        ("arena_mem", ctypes.c_void_p),
        ("arena_bytes", c_uint32),
        ("ref_mem", ctypes.c_void_p),
        ("ref_count", c_uint32),
        ("catalog", POINTER(HopperCatalog)),
    ]

lib.hopper_sizeof.restype = c_size_t
lib.hopper_ref_entry_sizeof.restype = c_size_t
lib.hopper_version.restype = c_uint32
lib.hopper_init.restype = c_int32
lib.hopper_record.restype = HopperResultRef
lib.hopper_field_set_bytes.restype = c_int32
lib.hopper_field_get_bytes.restype = c_int32

def main():
    print("Hopper ABI version", lib.hopper_version())
    ctx_size = lib.hopper_sizeof()
    ref_entry = lib.hopper_ref_entry_sizeof()

    arena = (c_uint8 * 64)()
    refs = (c_uint8 * (ref_entry * 4))()
    ctx = (c_uint8 * ctx_size)()

    pic = HopperPic(digits=3, scale=0, is_signed=0, usage=1, mask_ascii=None, mask_len=0)
    fields = (HopperField * 1)(
        HopperField(
            name_ascii=b"raw",
            name_len=3,
            offset=0,
            size=3,
            kind=1,  # bytes
            pad_byte=32,
            pic=pic,
            redefines_index=-1,
        )
    )
    layout = HopperLayout(
        name_ascii=b"Py",
        name_len=2,
        record_bytes=3,
        layout_id=1,
        fields=fields,
        field_count=1,
    )
    catalog = HopperCatalog(abi_version=1, layouts=ctypes.pointer(layout), layout_count=1)
    cfg = HopperConfig(
        abi_version=1,
        arena_mem=ctypes.cast(arena, ctypes.c_void_p),
        arena_bytes=64,
        ref_mem=ctypes.cast(refs, ctypes.c_void_p),
        ref_count=4,
        catalog=ctypes.pointer(catalog),
    )
    hopper_ptr = ctypes.c_void_p()
    err = lib.hopper_init(ctypes.cast(ctx, ctypes.c_void_p), ctypes.byref(cfg), ctypes.byref(hopper_ptr))
    if err != 0:
        raise SystemExit(f"init failed: {err}")
    res = lib.hopper_record(ctypes.cast(hopper_ptr, ctypes.c_void_p), 1)
    if res.ok != 1:
        raise SystemExit(f"alloc failed: {res.err}")
    data = (c_uint8 * 3)(*b"abc")
    rc = lib.hopper_field_set_bytes(ctypes.cast(hopper_ptr, ctypes.c_void_p), res.ref, 0, HopperBytes(data, 3))
    if rc != 0:
        raise SystemExit(f"set bytes failed: {rc}")
    out = (c_uint8 * 3)()
    rc = lib.hopper_field_get_bytes(ctypes.cast(hopper_ptr, ctypes.c_void_p), res.ref, 0, HopperBytesMut(out, 3))
    if rc != 0:
        raise SystemExit(f"get bytes failed: {rc}")
    print("bytes:", bytes(out).decode())

if __name__ == "__main__":
    main()
