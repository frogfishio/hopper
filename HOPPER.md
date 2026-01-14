<!-- SPDX-FileCopyrightText: 2026 Frogfish -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Author: Alexander Croft <alex@frogfish.io> -->

# Hopper Host Specification

*Grace Hopper taught the world that languages should serve people, not the other way around. Hopper is a small homage to that idea: structured records you can trust, sitting on top of a fixed arena you can reason about.*

This document is the **normative** host/runtime specification for Hopper.

- The Zing standard library declares Hopper intrinsics.
- The compiler lowers those intrinsics into calls.
- The **host/runtime** (e.g., `libzingrt` / `libzingcore` / embedded runtime) must provide the backing arena, reference table, and record operations defined here.

If you implement this spec faithfully, Hopper programs should behave identically across hosts, and Hopper record layouts should be wire-compatible by construction.

---

## 1. Design Goals

### 1.1 Goals (MUST)

1. **Determinism**: Hopper operations MUST be deterministic. No clocks, randomness, locale, address leakage, or host-dependent formatting.
2. **Predictable memory**: Allocation MUST be bounded, constant-policy, and arena-based. No hidden heap allocations.
3. **Wire-accurate layouts**: Field offsets and sizes MUST be honored exactly. No implicit padding, alignment surprises, or endian ambiguity.
4. **Safety by indirection**: Clients operate on **references**, not raw pointers. All reads/writes MUST bounds-check.
5. **C ABI friendliness**: The runtime ABI MUST be callable from C and other languages. Zing compilation targets a C-like calling convention.

### 1.2 Non-goals (MUST NOT)

- Hopper MUST NOT provide a general-purpose GC or free-list allocator.
- Hopper MUST NOT require type reflection, RTTI, or dynamic dispatch.
- Hopper MUST NOT bake in BigInt or any domain type. Hopper is **type-agnostic**; it merely stores bytes with rules.

---

## 2. Terms

- **Arena**: a contiguous slab of zero-initialized bytes from which records are bump-allocated.
- **Ref table**: an indexable table of record descriptors.
- **HopperRef**: an opaque handle (an integer) that indexes the ref table.
- **Layout**: a compile-time record definition with explicit size and a catalog of fields.
- **Field**: a named region within a layout with offset, width, and an interpretation (bytes, DISPLAY digits, COMP, COMP-3, etc.).
- **PIC**: a COBOL-inspired picture string that specifies numeric/text representations and optional edit masks.

---

## 3. Data Model

### 3.1 HopperRef

- `HopperRef` is an integer handle.
- A `HopperRef` MUST NOT be treated as a pointer.
- A `HopperRef` MUST resolve to either:
  - a valid ref table entry, or
  - an invalid reference error.

> **Recommended ABI type**: `int32_t`.

### 3.2 Arena

- The runtime MUST expose one arena instance per process (or per runtime instance if embedded).
- The arena is a contiguous byte slab of size `HOPPER_ARENA_SIZE`.
- The arena MUST be initialized to all-zero bytes at program start.
- Allocation policy is **bump allocation**:
  - an internal cursor points to the next free byte
  - allocating `N` bytes advances the cursor by `N`
  - individual records are never freed

### 3.3 Ref Table

- The runtime MUST expose a ref table containing exactly `HOPPER_REF_COUNT` entries.
- Each entry stores:
  - `ptr` (or arena offset),
  - `size_bytes`,
  - `layout_id`.

**Important**: Even if the implementation stores a raw pointer internally, HopperRef MUST remain opaque to callers.

### 3.4 Layout Catalog

The compiler assigns each `layout` a stable `layout_id` (within the compiled artifact).

For each layout, the runtime must receive (directly or indirectly) a **layout catalog** entry containing:

- total record size in bytes
- for each field:
  - `offset` (byte offset from record base)
  - `size` (field width in bytes)
  - `kind` (BYTES, NUMERIC_DISPLAY, COMP, COMP3, RAW_BYTES, EDIT_MASK, etc.)
  - `signed` (boolean)
  - `digits` (precision)
  - `scale` (implied decimal digits)
  - `pic` string (if applicable)
  - edit mask metadata (if applicable)
  - overlay/redefines relationships (informational; the offset/size rules still govern)

**Catalog delivery**: The host may embed the catalog into the compiled binary (recommended), or accept it through a registration API at startup. Either way, operations MUST behave as if the catalog is immutable.

---

## 4. Error Model

Hopper is designed to be used by languages with different error styles. This spec defines error *categories* and allows hosts to choose a representation.

### 4.1 Error Categories

Implementations MUST distinguish (at least) the following categories:

- `HOPPER_OK`
- `HOPPER_ERR_OOM` — arena cannot satisfy allocation
- `HOPPER_ERR_REF_LIMIT` — ref table has no free slots
- `HOPPER_ERR_LAYOUT_UNKNOWN` — invalid layout id
- `HOPPER_ERR_INVALID_REF` — HopperRef does not reference a valid entry
- `HOPPER_ERR_BOUNDS` — offset/width out of bounds
- `HOPPER_ERR_PIC_INVALID` — invalid digit/sign nibble, malformed field contents
- `HOPPER_ERR_SCALE_MISMATCH` — scale rule violated
- `HOPPER_ERR_OVERFLOW` — value too large for declared digits/width
- `HOPPER_ERR_DST_TOO_SMALL` — destination buffer too small for requested output

### 4.2 Fault vs. Result

- **Raw byte intrinsics** MAY trap/fault on bounds errors if that is how the compiler expects to surface them.
- All other operations SHOULD return explicit error codes.

If a bounds error is surfaced via trap/fault, the trap MUST be deterministic and MUST NOT leak host addresses.

---

## 5. Required Host ABI

This section is normative. Names are illustrative; **semantics are required**.

> **C ABI recommended**: arguments in registers / stack per platform ABI, return in `w0/x0`.

### 5.1 Configuration Constants

The runtime MUST define (or export) these constants:

- `HOPPER_ARENA_SIZE`
- `HOPPER_REF_COUNT`

The runtime MUST also define the size of one ref-table entry; recommended internal layout:

- `ptr_or_off` (u32 or pointer)
- `size_bytes` (u32)
- `layout_id` (u32)

### 5.2 Allocation / Reference Creation

**Operation**: allocate a new record instance.

- `hopper_alloc(layout_id, size_bytes, out_ref, out_err)`

Semantics:

1. Validate `layout_id` exists and matches `size_bytes`.
   - On mismatch: `HOPPER_ERR_LAYOUT_UNKNOWN` or `HOPPER_ERR_PIC_INVALID` (implementation-defined), but MUST be deterministic.
2. Ensure `cursor + size_bytes <= HOPPER_ARENA_SIZE`.
   - Else: `HOPPER_ERR_OOM`.
3. Ensure there is a free ref table slot.
   - Else: `HOPPER_ERR_REF_LIMIT`.
4. Bump-allocate from arena; zero-fill `size_bytes` bytes.
5. Populate ref entry with `{ptr/off, size_bytes, layout_id}`.
6. Return the newly assigned `HopperRef`.

**Zero-fill** is required and MUST be deterministic.

### 5.3 Bounds Helper

- `hopper_bounds_ok(ref, offset, width) -> int`

Returns 1 if `(offset + width) <= size_bytes` for the record, else 0.

This helper MUST validate the ref first; invalid refs MUST return 0.

### 5.4 Raw Byte Access (HopperBytes intrinsics)

These provide byte-level control. They MUST bounds-check.

- `hopper_read_u8(ref, off, out_err) -> u8`
- `hopper_read_u16le(ref, off, out_err) -> u16`
- `hopper_read_u32le(ref, off, out_err) -> u32`

- `hopper_write_u8(ref, off, v) -> err`
- `hopper_write_u16le(ref, off, v) -> err`
- `hopper_write_u32le(ref, off, v) -> err`

Endianness:

- `u16le` and `u32le` MUST be little-endian regardless of host endianness.

On error:

- If using explicit error returns: set `out_err` and return 0.
- If using traps for bounds: the trap MUST be deterministic and MUST correspond to `HOPPER_ERR_BOUNDS`.

### 5.5 Field Operations

Field operations interpret bytes according to field metadata.

All field operations MUST:

1. Validate `ref` is valid.
2. Validate `(field.offset + field.size) <= record.size_bytes`.
3. Operate only within the declared field region.

#### 5.5.1 Bytes get/set

- `hopper_get_bytes(ref, field, dst, dst_cap, out_len, out_err)`
  - Copies exactly `field.size` bytes into `dst` if `dst_cap >= field.size`.
  - Else: `HOPPER_ERR_DST_TOO_SMALL`.

- `hopper_set_bytes(ref, field, src, src_len) -> err`
  - If `src_len > field.size`: `HOPPER_ERR_PIC_INVALID` (or `HOPPER_ERR_OVERFLOW`), MUST NOT truncate.
  - If `src_len < field.size`: MUST pad deterministically with ASCII space (0x20) unless the field kind requires different padding.

- `hopper_set_raw_bytes(ref, field, src, src_len) -> err`
  - Requires `src_len == field.size`.
  - Else: `HOPPER_ERR_PIC_INVALID`.
  - Copies bytes verbatim.

#### 5.5.2 DISPLAY numeric get/set

- `hopper_get_numeric(ref, field, out_i64) -> err`
- `hopper_set_numeric(ref, field, i64_value) -> err`

DISPLAY numeric format rules:

- Digits are stored as ASCII '0'..'9'.
- Signed fields MAY include a sign byte ('+' or '-') in a defined position. The catalog MUST specify the exact representation.
- If any digit is invalid: `HOPPER_ERR_PIC_INVALID`.
- Scale rules:
  - If `field.scale > 0`, the stored value is interpreted as a scaled integer.
  - If scale mismatch is detectable (e.g., trailing digits not divisible by 10^scale where required by an operation): `HOPPER_ERR_SCALE_MISMATCH`.
- Overflow rules:
  - If the numeric magnitude cannot fit within declared digits: `HOPPER_ERR_OVERFLOW`.

#### 5.5.3 COMP (binary) get/set

- `hopper_get_comp(ref, field, out_i64) -> err`
- `hopper_set_comp(ref, field, i64_value) -> err`

Rules:

- COMP values are stored as two's complement integers in a fixed width declared by the field size.
- Endianness MUST be specified by the catalog; recommended: little-endian.
- Scale rules apply as in DISPLAY.

#### 5.5.4 COMP-3 (packed decimal) get/set

- `hopper_get_comp3(ref, field, out_i64) -> err`
- `hopper_set_comp3(ref, field, i64_value) -> err`

Rules:

- Packed decimal stores two digits per byte; the final nibble is the sign.
- Sign nibbles MUST accept:
  - 0xC = positive
  - 0xD = negative
  - 0xF = unsigned/positive
- Any invalid nibble: `HOPPER_ERR_PIC_INVALID`.
- Precision and scale MUST be enforced; overflow MUST return `HOPPER_ERR_OVERFLOW`.

### 5.6 PIC Display / Compare / Validate

These are needed for formatter and conformance tests.

#### 5.6.1 Display

- `hopper_pic_display(ref, field, dst, dst_cap, out_len, out_err)`

Semantics:

- Formats according to PIC string and edit mask rules.
- MUST be deterministic.
- MUST error on:
  - invalid digit/sign (`HOPPER_ERR_PIC_INVALID`)
  - scale mismatch (`HOPPER_ERR_SCALE_MISMATCH`)
  - overflow (`HOPPER_ERR_OVERFLOW`)
  - insufficient output space (`HOPPER_ERR_DST_TOO_SMALL`)

#### 5.6.2 Compare

- `hopper_pic_compare(ref, field, rhs_value, out_cmp, out_err)`

Semantics:

- Compares field value to `rhs_value` under PIC rules.
- Writes `out_cmp` as -1, 0, +1.
- Errors as per display/validate.

#### 5.6.3 Validate

- `hopper_pic_validate(ref, field, out_err)`

Semantics:

- Validates that stored bytes satisfy the PIC constraints.
- MUST NOT allocate.
- MUST NOT write output.

---

## 6. Overlays / REDEFINES

Hopper supports multiple field views over the same byte region.

- Overlays are expressed in the layout catalog (fields may share offsets).
- The runtime MUST NOT attempt to “resolve” overlays.
- Each operation MUST treat the field definition as authoritative:
  - bounds are checked against that field's `offset+size`
  - encoding/decoding is performed using that field's kind and PIC metadata

This yields the intended COBOL-like behavior: **one region of bytes, many correct views**.

---

## 7. Runtime Initialization

At program start (or runtime instance init):

- Arena MUST be zeroed.
- Ref table MUST be zeroed and considered empty.
- Cursor MUST be set to 0.
- Next free ref slot index MUST be set to 0.

Re-initialization (reset) MAY exist, but if present it MUST be explicit and deterministic.

---

## 8. Determinism & Observability Rules

Implementations MUST ensure:

- No operation exposes host pointers.
- No formatted output depends on locale.
- No undefined behavior depends on C integer overflow.
- No timing-based behavior.

If debugging hooks exist, they MUST be opt-in and MUST NOT change semantics.

---

## 9. Conformance Requirements

A host implementation is conformant if it satisfies:

- allocation success/failure semantics (`OOM`, `REF_LIMIT`)
- bounds behavior for all reads/writes
- correct PIC digit/sign validation
- correct COMP/COMP-3 behavior and overflow handling
- deterministic formatting and comparison
- overlay correctness

### 9.1 Test Coverage Expectations

A conforming runtime MUST support the following test categories:

- Allocation OK/OOM/ref-limit
- Field read/write: bytes, numeric, COMP, COMP-3
- PIC display / compare / validate
- Edit masks and sign rules
- Overlay/redefines
- Invalid ref and bounds faults

---

## 10. Portability Notes

- The ABI MUST be stable across platforms.
- Fixed-width integer types SHOULD be used in the C interface.
- Endianness-sensitive operations MUST specify endianness (LE recommended) and obey it regardless of host.

---

## 11. Versioning

This spec is versioned.

- A conforming implementation SHOULD report a Hopper version constant.
- Incompatible changes MUST bump the major version.

---

## Appendix A — Suggested Minimal C ABI (Illustrative)

The following is an illustrative naming scheme. Implementations may choose different names as long as semantics match.

- `int hopper_alloc(uint32_t layout_id, uint32_t size_bytes, int32_t *out_ref);`
- `int hopper_bounds_ok(int32_t ref, uint32_t off, uint32_t width);`
- `uint8_t  hopper_read_u8(int32_t ref, uint32_t off, int *err);`
- `uint16_t hopper_read_u16le(int32_t ref, uint32_t off, int *err);`
- `uint32_t hopper_read_u32le(int32_t ref, uint32_t off, int *err);`
- `int hopper_write_u8(int32_t ref, uint32_t off, uint8_t v);`
- `int hopper_write_u16le(int32_t ref, uint32_t off, uint16_t v);`
- `int hopper_write_u32le(int32_t ref, uint32_t off, uint32_t v);`

And field-driven operations:

- `int hopper_get_bytes(int32_t ref, const HopperField *f, uint8_t *dst, uint32_t cap, uint32_t *out_len);`
- `int hopper_set_bytes(int32_t ref, const HopperField *f, const uint8_t *src, uint32_t len);`
- `int hopper_set_raw_bytes(int32_t ref, const HopperField *f, const uint8_t *src, uint32_t len);`

- `int hopper_get_numeric(int32_t ref, const HopperField *f, int64_t *out);`
- `int hopper_set_numeric(int32_t ref, const HopperField *f, int64_t v);`

- `int hopper_get_comp(int32_t ref, const HopperField *f, int64_t *out);`
- `int hopper_set_comp(int32_t ref, const HopperField *f, int64_t v);`

- `int hopper_get_comp3(int32_t ref, const HopperField *f, int64_t *out);`
- `int hopper_set_comp3(int32_t ref, const HopperField *f, int64_t v);`

PIC helpers:

- `int hopper_pic_display(int32_t ref, const HopperField *f, char *dst, uint32_t cap, uint32_t *out_len);`
- `int hopper_pic_compare(int32_t ref, const HopperField *f, const HopperPicValue *rhs, int *out_cmp);`
- `int hopper_pic_validate(int32_t ref, const HopperField *f);`

---
