# SPDX-FileCopyrightText: 2026 Frogfish
# SPDX-License-Identifier: Apache-2.0
# Author: Alexander Croft <alex@frogfish.io>

# Python binding sketch

This is a minimal ctypes example to call Hopper. It assumes `libhopper.so` is installed and on the loader path.

```python
import ctypes
from ctypes import c_int32, c_uint32, c_uint8, c_size_t, POINTER, Structure

lib = ctypes.CDLL("libhopper.so")

class HopperResultU32(Structure):
    _fields_ = [("ok", c_int32), ("err", c_int32), ("v", c_uint32)]

lib.hopper_sizeof.restype = c_size_t
lib.hopper_ref_entry_sizeof.restype = c_size_t
lib.hopper_version.restype = c_uint32

print("Hopper ABI version:", lib.hopper_version())
```

For full bindings, mirror the structs in `hopper.h` and load the library with `ctypes` or `cffi`. See `doc/bindings.md` for guidance.
