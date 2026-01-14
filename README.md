<!-- SPDX-FileCopyrightText: 2026 Frogfish -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Author: Alexander Croft <alex@frogfish.io> -->

# HOPPER

*Grace Hopper taught the world that languages should serve people, not the other way around. Hopper is a small homage to that idea: structured records you can trust, sitting on top of a fixed arena you can reason about.*

Current release: 1.0.0 (ABI version 1).

Hopper is an **arena-backed record system** with **explicit layouts** (offsets, sizes, encodings) and **safe access** (bounds checks, typed reads/writes). It’s designed for the part of systems programming where you want **byte-for-byte control** without falling back to “raw pointer soup”.

Think: *mainframe-style record discipline*, but as a small, embeddable library.

---

## Why Hopper exists

Most environments give you two extremes:

- **Raw bytes**: fast and flexible, but easy to mis-index, mis-size, or accidentally reinterpret.
- **Heap objects**: ergonomic, but with hidden allocation, fragmentation, GC, and weak guarantees about wire layout.

Hopper lives in the middle:

- You allocate from a **fixed arena** (predictable memory and no fragmentation surprises).
- You work with **records** defined by layouts (predictable offsets, encodings, and sizes).
- Every read/write is **checked** (bounds, encoding validity) and **deterministic**.

This is useful in places where correctness and predictability beat cleverness:

- file formats and binary protocols
- financial records and fixed-point decimals
- structured telemetry/log envelopes
- language runtimes (safe “scratch space” without leaking raw pointers)
- tools that need stable, inspectable memory representations

---

## What Hopper is (and isn’t)

### It is

- A **safe arena**: allocate records, keep them until arena reset/free.
- A **layout engine**: define record fields with explicit offsets and encodings.
- A **codec layer**: `DISPLAY` digits, binary (`COMP`), packed decimal (`COMP-3`), raw bytes.
- A **portable runtime primitive**: usable from C, Zing, or any language that can call a C ABI.

### It is not

- A general-purpose heap / GC replacement.
- A dynamic object system.
- A serialization framework that guesses formats for you.

Hopper is intentionally boring: **everything is explicit**.

---

## Design principles

- **Deterministic by default**: no locale, no pointer/address observability, no hidden time/rand.
- **Data-oriented**: layouts describe bytes; APIs manipulate bytes through that description.
- **No ambient magic**: no implicit conversions; encodings are named and enforced.
- **Total failure modes**: operations return status/results; no “mystery half-writes”.
- **Embeddable**: small surface area, C ABI, easy to vendor.

---

## Core concepts

### Arena

An arena is a fixed block of memory. Allocations advance a cursor.

- allocations are **fast** (bump pointer)
- memory is **predictable** (bounded)
- individual frees are **not supported** (by design)

### HopperRef

Allocations return a small integer handle:

- not a pointer
- validated on access
- maps to an internal table entry: `(base_offset, size, layout_id)`

This indirection is the safety line: you can’t do pointer arithmetic and wander off.

### Layout

A layout names a record shape, including byte size and field definitions:

- explicit `at:` offsets
- explicit sizes / picture strings
- explicit storage mode (`DISPLAY`, `COMP`, `COMP3`, raw)

Layouts are how Hopper stays honest: if the layout says “salary at offset 33”, that is what it means, everywhere.

---

## The mental model

A Hopper record is **a view over bytes** that is checked and typed.

- Reading a numeric field means **decode bytes → integer** (with validation).
- Writing a numeric field means **integer → encode bytes** (with overflow/format checks).
- Reading/writing bytes means **copy with bounds checks**.

Hopper does *not* hide that these are bytes. It just makes them hard to misuse.

---

## Pictures & storage (PIC)

Hopper borrows COBOL’s picture strings because they’re a compact way to describe record fields.

Common forms:

- `X(n)` – n raw bytes
- `9(n)` – n digits
- `S9(n)` – signed digits
- `V` – implied decimal point (fixed-point scale)

Storage modes:

- `DISPLAY` – ASCII digits (human-readable)
- `COMP` – binary integer (little-endian)
- `COMP3` – packed decimal (BCD, sign nibble)

Important: fields with `V` are **scaled integers**. A `9(5)V99` field stores `123.45` as the integer `12345`.

---

## REDEFINES (overlays)

Sometimes you need multiple interpretations of the same bytes.

`redefines:` lets a field alias an existing region. It’s explicit and intentional — a safe version of a union.

Rules of thumb:

- Use overlays for headers/bodies, union-like variants, and “raw + structured view” patterns.
- Keep overlays obvious: prefer one `raw` field and a small set of named views.

---

## Getting started

This repo is early. The README explains the shape and goals first; the API is still stabilizing.

Expected project structure (target):

- `include/hopper.h` – public C API
- `src/` – arena + table + codec implementations
- `examples/` – small demos
- `tests/` – conformance tests (layouts, bounds, numeric codecs)

---

## Example: defining a layout (Zing-style)

*(This is illustrative. The library is language-agnostic; Zing bindings can map 1:1.)*

```zing
layout Person bytes: 50 {
  name:   at: 0  as: pic "X(30)".
  age:    at: 30 as: pic "9(3)" usage: #DISPLAY.
  salary: at: 33 as: pic "S9(9)V99" usage: #COMP3.
}.
```

---

## Example: record lifecycle

- allocate record → get `HopperRef`
- set fields (validated)
- read fields (validated)
- drop everything by resetting/freeing the arena

This is the “batch processing” happy path: fast, predictable, no cleanup per object.

---

## How Hopper fits with Zing

Zing wants a runtime story where:

- effects are explicit,
- memory is predictable,
- and raw pointers don’t leak into user programs.

Hopper is a natural fit as the **structured scratch space** for:

- numeric work (including big integer *users* without Hopper knowing anything about big integers)
- parsing and formatting pipelines
- deterministic tool output

The key point: **Hopper stays agnostic**. BigInt (or anything else) can *use* Hopper without Hopper embedding BigInt rules.

---

## Roadmap (pragmatic)

Near-term:

- stabilize the core C API (arena init/reset, record alloc, field read/write)
- stabilize PIC parsing and numeric codecs (DISPLAY/COMP/COMP3)
- define clear, testable error codes

Later:

- richer tooling around layouts (layout compiler, schema dump)
- bindings for Zing and other languages
- performance passes (while preserving semantics)

---

## Contributing

This project is meant to be small and sharp.

If you contribute:

- prefer explicit invariants over clever abstractions
- add tests for every new codec rule
- keep determinism guarantees intact

---

## License

See repository license files.
