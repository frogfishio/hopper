# TODO

What you need to add to the repo (minimum viable “real library”)

1) include/hopper.h — the ABI contract (frozen early)

This is the missing keystone. Everything else can iterate behind it.

2) src/hopper.c — arena + ref table + raw byte ops

No PIC yet, just allocation, ref validity, bounds checks, byte loads/stores.

3) src/pic.c + src/pic.h — PIC parsing helpers + DISPLAY/COMP/COMP-3

Keep Hopper agnostic of BigInt; PIC just outputs/consumes integers/bytes.

4) tests/ — conformance tests that lock semantics

Especially: COMP-3 sign nibble, edit-mask formatting, bounds, initialization rules.

5) Build system: simplest possible (Makefile + one static lib)

You want libhopper.a and later optionally libhopper.so / dylib.

⸻

The one key design decision (that removes 80% of ambiguity)

Don’t hardcode HOPPER_ARENA_SIZE and HOPPER_REF_COUNT.

Make them runtime parameters of a Hopper context.
Zing can still choose to compile with fixed defaults, but the library doesn’t force it.

That also makes it immediately attractive to C users.

⸻

Proposed C API (drop-in include/hopper.h draft)

This is intentionally “small but complete enough to implement”.

// SPDX-FileCopyrightText: 2026 Frogfish
// SPDX-License-Identifier: Apache-2.0
// Author: Alexander Croft <alex@frogfish.io>

#ifndef HOPPER_H
#define HOPPER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ------------------------------
// Versioning
// ------------------------------
#define HOPPER_ABI_VERSION 1u

// ------------------------------
// Core types
// ------------------------------

// Opaque handle (never a pointer).
typedef int32_t hopper_ref_t;

// Stable error codes (no strings required for ABI).
typedef enum hopper_err_e {
  HOPPER_OK = 0,

  // Allocation / capacity
  HOPPER_E_OOM_ARENA = 1,
  HOPPER_E_OOM_REFS  = 2,

  // Ref / bounds
  HOPPER_E_BAD_REF   = 3,
  HOPPER_E_BOUNDS    = 4,

  // Layout / field
  HOPPER_E_BAD_LAYOUT = 5,
  HOPPER_E_BAD_FIELD  = 6,

  // PIC / encoding
  HOPPER_E_PIC_INVALID = 7,
  HOPPER_E_PIC_SCALE   = 8,
  HOPPER_E_OVERFLOW    = 9,

  // Generic
  HOPPER_E_UNSUPPORTED = 10,
} hopper_err_t;

// A tiny Result type that stays C ABI-friendly.
typedef struct hopper_result_i32_s {
  int32_t ok;        // 1 = Ok, 0 = Fail
  hopper_err_t err;  // valid if ok==0
  int32_t v;         // valid if ok==1
} hopper_result_i32_t;

typedef struct hopper_result_u32_s {
  int32_t ok;
  hopper_err_t err;
  uint32_t v;
} hopper_result_u32_t;

typedef struct hopper_result_ref_s {
  int32_t ok;
  hopper_err_t err;
  hopper_ref_t ref;
} hopper_result_ref_t;

// Non-owning byte view.
typedef struct hopper_bytes_s {
  const uint8_t *ptr;
  uint32_t len;
} hopper_bytes_t;

// Mutable byte view.
typedef struct hopper_bytes_mut_s {
  uint8_t *ptr;
  uint32_t len;
} hopper_bytes_mut_t;

// ------------------------------
// Layout descriptors
// ------------------------------

// Storage/encoding.
typedef enum hopper_usage_e {
  HOPPER_USAGE_DISPLAY = 1, // ASCII digits, optional sign (+/-) if signed
  HOPPER_USAGE_COMP    = 2, // binary (i16/i32) little-endian
  HOPPER_USAGE_COMP3   = 3, // packed BCD (COMP-3)
} hopper_usage_t;

// Field kind: bytes or numeric.
// (You can extend later without breaking ABI by adding new kinds.)
typedef enum hopper_field_kind_e {
  HOPPER_FIELD_BYTES  = 1,
  HOPPER_FIELD_NUM_I32 = 2, // numeric exposed as i32 scaled/unscaled per PIC
} hopper_field_kind_t;

// PIC metadata is pre-parsed by tooling/compile-time.
// Hopper does NOT need to parse PIC strings at runtime unless you want it.
typedef struct hopper_pic_s {
  uint16_t digits;     // total digits
  uint16_t scale;      // digits after V (implied decimal)
  uint8_t  is_signed;  // 0/1
  uint8_t  usage;      // hopper_usage_t

  // Optional edit mask:
  // If mask_len==0 => no mask.
  const char *mask_ascii; // bytes of mask (not necessarily NUL-terminated)
  uint16_t mask_len;
} hopper_pic_t;

typedef struct hopper_field_s {
  const char *name_ascii; // stable field name (ASCII), optional for tooling
  uint16_t name_len;

  uint32_t offset;        // byte offset in record
  uint32_t size;          // field storage bytes

  hopper_field_kind_t kind;

  // For bytes fields:
  uint8_t pad_byte;       // when setting shorter bytes, pad with this (usually space)

  // For numeric fields:
  hopper_pic_t pic;

  // Overlay:
  int32_t redefines_index; // -1 if none; otherwise index into same layout fields[]
} hopper_field_t;

typedef struct hopper_layout_s {
  const char *name_ascii;
  uint16_t name_len;

  uint32_t record_bytes;     // total record size
  uint32_t layout_id;        // stable small id used by ref table
  const hopper_field_t *fields;
  uint32_t field_count;
} hopper_layout_t;

// Catalog = list of layouts. Owned by caller; Hopper just references it.
typedef struct hopper_catalog_s {
  uint32_t abi_version; // must be HOPPER_ABI_VERSION
  const hopper_layout_t *layouts;
  uint32_t layout_count;
} hopper_catalog_t;

// ------------------------------
// Hopper context
// ------------------------------

typedef struct hopper_s hopper_t;

// Create a Hopper context using caller-provided memory.
// - arena_mem: raw bytes (arena_bytes length)
// - ref_mem:   table entries memory (ref_count entries)
// This design makes Hopper embeddable in anything (no malloc required).
typedef struct hopper_config_s {
  uint32_t abi_version;   // HOPPER_ABI_VERSION
  void    *arena_mem;
  uint32_t arena_bytes;

  void    *ref_mem;       // array of ref entries (opaque to user)
  uint32_t ref_count;

  const hopper_catalog_t *catalog; // may be NULL (raw-only mode)
} hopper_config_t;

// Initializes a hopper_t in caller-provided storage.
// hopper_storage must be at least hopper_sizeof().
size_t hopper_sizeof(void);
hopper_err_t hopper_init(void *hopper_storage, const hopper_config_t *cfg, hopper_t **out);

// Resets arena cursor + ref allocation pointer.
// Does NOT wipe arena bytes unless asked.
hopper_err_t hopper_reset(hopper_t *h, int32_t wipe_arena);

// ------------------------------
// Allocation & ref queries
// ------------------------------

// Allocate a record of layout_id.
// - If catalog is NULL => HOPPER_E_BAD_LAYOUT
hopper_result_ref_t hopper_record(hopper_t *h, uint32_t layout_id);

// Validate a ref and return (offset,size,layout_id) if valid.
typedef struct hopper_ref_info_s {
  uint32_t offset;
  uint32_t size;
  uint32_t layout_id;
} hopper_ref_info_t;

int32_t hopper_ref_info(hopper_t *h, hopper_ref_t ref, hopper_ref_info_t *out);

// ------------------------------
// Raw byte access (always available)
// ------------------------------
hopper_result_u32_t hopper_read_u8 (hopper_t *h, hopper_ref_t ref, uint32_t off);
hopper_result_u32_t hopper_read_u16le(hopper_t *h, hopper_ref_t ref, uint32_t off);
hopper_result_u32_t hopper_read_u32le(hopper_t *h, hopper_ref_t ref, uint32_t off);

hopper_err_t hopper_write_u8 (hopper_t *h, hopper_ref_t ref, uint32_t off, uint8_t  v);
hopper_err_t hopper_write_u16le(hopper_t *h, hopper_ref_t ref, uint32_t off, uint16_t v);
hopper_err_t hopper_write_u32le(hopper_t *h, hopper_ref_t ref, uint32_t off, uint32_t v);

// ------------------------------
// Field access (requires catalog)
// ------------------------------

// Bytes fields.
// - If input shorter than field size: pad with field.pad_byte.
// - If input longer: fail with HOPPER_E_PIC_INVALID (or a dedicated length error if you want).
hopper_err_t hopper_field_set_bytes(hopper_t *h, hopper_ref_t ref, uint32_t field_index, hopper_bytes_t bytes);
hopper_err_t hopper_field_get_bytes(hopper_t *h, hopper_ref_t ref, uint32_t field_index, hopper_bytes_mut_t out);

// Numeric fields exposed as i32 (scaled integer).
hopper_result_i32_t hopper_field_get_i32(hopper_t *h, hopper_ref_t ref, uint32_t field_index);
hopper_err_t         hopper_field_set_i32(hopper_t *h, hopper_ref_t ref, uint32_t field_index, int32_t v);

// DISPLAY edit-mask formatting into out buffer (caller supplies output bytes).
// Returns number of bytes written (mask length) or Fail.
hopper_result_i32_t hopper_field_format_display(hopper_t *h, hopper_ref_t ref, uint32_t field_index, hopper_bytes_mut_t out);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // HOPPER_H

This header answers all of Codex’s blockers:
	•	✅ concrete API
	•	✅ layout/catalog representation
	•	✅ no hardcoded constants (they become config)
	•	✅ conformance-testable behavior
	•	✅ still matches Zing’s worldview (ref table + arena + deterministic errors)

⸻

How this maps to Zing (important sanity check)

Your codegen snippet is already doing:
	•	cursor bump + bounds
	•	ref table slot
	•	write (offset,size,layout_id)
	•	fill zero

That becomes an implementation of hopper_record() with:
	•	layout_id resolved from catalog->layouts[]
	•	record_bytes from layout descriptor

So Zing can either:
	•	keep generating “inline hopper” today, then later switch to calling hopper_record() (cleaner),
	•	or start calling Hopper immediately once the ABI exists.

Either way, the semantics freeze now.

⸻

Conformance tests that will make this “real”

Add these first (they catch 90% of future regret):
	1.	Allocation / reset

	•	allocate N records until ref_count exhausted
	•	allocate until arena exhausted
	•	reset with wipe/no-wipe behavior

	2.	Bounds

	•	read/write at end-1 OK, at end fail
	•	validate bad ref returns BAD_REF

	3.	DISPLAY numeric

	•	signed: leading + or - handling
	•	scale mismatch rule (if you keep it)
	•	non-digit fails

	4.	COMP (binary)

	•	endianness locked (LE)
	•	scale division exactness rule

	5.	COMP-3

	•	pack/unpack digits
	•	sign nibble: C=pos, D=neg, F=unsigned accepted?
	•	pick and freeze.

	6.	Edit masks

	•	Z suppression + comma/decimal insertion
	•	deterministic output length == mask_len

This is what makes IBM-believable: not “features”, locked semantics + tests.

⸻

What I’d do next (very concrete)
	1.	Add include/hopper.h exactly like above (or with your naming tweaks).
	2.	Implement only:
	•	init/reset
	•	record allocation
	•	raw reads/writes
	3.	Add one tiny catalog + one layout in tests, prove:
	•	record alloc works
	•	field_set_bytes/get_bytes works
	4.	Then implement DISPLAY numeric (the simplest PIC path) and tests.
	5.	Then COMP3 (most “signature” Hopper feature).

⸻

If you paste your current HOPPER.md normative rules (or point to the key parts), I can adapt the header so error codes + scale rules + sign policy match your exact semantics, not my guesses. But the header shape above is the decision that unblocks everything.