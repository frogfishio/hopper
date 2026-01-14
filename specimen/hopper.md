# Hopper: Arena-Based Structured Memory

**The Hopper system provides fixed-size, structured memory allocation with COBOL-inspired layouts. It brings the precision of mainframe record definitions to modern arena allocation.**

---

## Why Hopper?

Most languages give you two extremes: raw byte buffers with no structure, or heap-allocated objects with garbage collection. Neither works well when you need:

- **Predictable memory usage** — You know exactly how many records fit in your arena
- **Wire-compatible layouts** — Data structures that match external formats byte-for-byte
- **Packed decimal arithmetic** — Financial calculations without floating-point errors
- **No GC pauses** — All memory comes from a fixed arena

Hopper fills this gap. It's an arena allocator that understands *structure*—each allocation creates a record with typed fields, automatic initialization, and bounds checking.

The design draws from COBOL's PIC (Picture) clause system, which has processed trillions of dollars of financial transactions since the 1960s. If you've ever wondered how banks maintain penny-perfect accuracy across millions of records, this is the foundation.

---

## Core Concepts

### The HopperRef

Every allocation returns a `HopperRef`—a handle to your record within the arena:

```zing
type HopperRef = i32.
```

This is an opaque reference, not a pointer. You can't do arithmetic on it or dereference it directly. The runtime maintains a table mapping each ref to its arena location, size, and layout type.

Why this indirection? Safety. A raw pointer could wander anywhere in memory. A `HopperRef` always refers to a valid, properly-typed record or nothing at all.

### Layouts

Before allocating, you define what you're allocating. A **layout** declares the structure of your records:

```zing
layout Person bytes: 50 {
    name: at: 0 as: pic "X(30)".
    age:  at: 30 as: pic "9(3)" usage: #DISPLAY.
    salary: at: 33 as: pic "S9(9)V99" usage: #COMP3.
}.
```

This declares a 50-byte record with three fields:
- `name` — 30 bytes of alphanumeric data starting at offset 0
- `age` — 3-digit display numeric starting at offset 30
- `salary` — signed 9.2-digit packed decimal starting at offset 33

Every field has an explicit position (`at:`) and format (`as: pic`). No padding surprises, no alignment games. You know exactly where every byte lives.

### Allocation

To create a record:

```zing
let rec = Hopper record: #Person.
```

This returns a `Result[HopperRef, Err]`. The arena has finite space (20,000 bytes by default) and a maximum reference count (64 slots). Allocation can fail:

```zing
Result if rec {
    ok person => {
        -- Use person
    }
    fail e => {
        -- Handle out-of-memory
    }
}.
```

On success, the entire record is zero-initialized. Display fields become spaces, numeric fields become zeros.

---

## The PIC Clause: A Primer

PIC (Picture) clauses describe data formats using a pattern language. Each character in the pattern represents one byte or digit:

| Symbol | Meaning |
|--------|---------|
| `9` | Numeric digit (0-9) |
| `X` | Any character (alphanumeric) |
| `S` | Sign indicator (positive/negative) |
| `V` | Implied decimal point |
| `(n)` | Repeat previous symbol n times |

### Numeric Pictures

```zing
pic "9(5)"      -- 5 digits: 00000 to 99999
pic "S9(7)"     -- Signed 7 digits: -9999999 to +9999999  
pic "9(5)V99"   -- 5.2 digits: 00000.00 to 99999.99
pic "S9(9)V99"  -- Signed 9.2: -999999999.99 to +999999999.99
```

The `V` doesn't occupy space—it marks where the decimal point falls when you read the value as an integer. A field with `9(5)V99` stores the value 12345.67 as the integer 1234567.

### Alphanumeric Pictures

```zing
pic "X(20)"     -- 20 characters of anything
pic "X(100)"    -- 100-byte text buffer
```

Alphanumeric fields store raw bytes. Reading returns a `Bytes` value; writing accepts `Bytes`.

---

## Usage Modes

The same PIC can be stored different ways. The `usage:` clause controls encoding:

### DISPLAY (Default)

Each digit occupies one byte as its ASCII value:

```zing
layout Receipt bytes: 10 {
    amount: at: 0 as: pic "9(5)V99" usage: #DISPLAY.
}.
```

The value 123.45 stores as: `0x30 0x30 0x31 0x32 0x33 0x34 0x35` ("0012345" in ASCII).

**Use DISPLAY when:**
- Human readability matters (logs, debugging)
- Interfacing with text-based protocols
- You need byte-for-byte control

### COMP-3 (Packed Decimal)

Two digits per byte, with sign in the low nibble of the last byte:

```zing
layout Transaction bytes: 8 {
    amount: at: 0 as: pic "S9(9)V99" usage: #COMP3.
}.
```

The value -12345.67 packs into 6 bytes: `0x01 0x23 0x45 0x67 0x0D`

The trailing nibble encodes sign:
- `0xC` = positive
- `0xD` = negative
- `0xF` = unsigned

**Use COMP-3 when:**
- Space efficiency matters (nearly 2x denser than DISPLAY)
- Processing financial data
- Interfacing with mainframe systems

### COMP (Binary)

Native binary integer, little-endian:

```zing
layout Counter bytes: 4 {
    count: at: 0 as: pic "9(5)" usage: #COMP.
}.
```

Stores as a 4-byte i32 in machine byte order.

**Use COMP when:**
- Maximum arithmetic performance matters
- Interfacing with binary protocols
- You don't need the decimal precision of COMP-3

---

## Field Access

Fields generate getter and setter methods automatically. Given:

```zing
layout Account bytes: 20 {
    balance: at: 0 as: pic "S9(9)V99" usage: #COMP3.
    holder: at: 6 as: pic "X(14)".
}.
```

You can read and write:

```zing
Result if Hopper record: #Account {
    ok acct => {
        -- Write to fields (setters return Result[Unit, Err])
        Result if balance(acct, 1000_00) {
            ok _ => {
                -- Set holder
                Result if holder(acct, "Alice Smith   ") {
                    ok _ => {
                        -- Read back
                        let bal = balance(acct).  -- Returns Result[i32, Err]
                        let name = holder(acct).  -- Returns Result[Bytes, Err]
                    }
                    fail e => error(e).
                }.
            }
            fail e => error(e).
        }.
    }
    fail e => error(e).
}.
```

Notice the setter syntax: `field(ref, value)` writes; `field(ref)` reads. Both operations return `Result` because they can fail on bounds violations.

### Numeric Field Values

For fields with `V` (implied decimal), values are scaled integers:

```zing
-- Field: pic "9(5)V99"
-- To store 123.45, pass 12345
amount(rec, 12345).

-- Reading returns 12345, not 123.45
let val = amount(rec).  -- val = 12345
```

You're responsible for tracking the decimal position. This matches how COBOL handles fixed-point arithmetic—the compiler knows the scale, but values are always integers.

---

## Edit Masks

For formatted output, use edit mask pictures:

```zing
layout Report bytes: 15 {
    amount: at: 0 as: pic "ZZ,ZZ9.99".
}.
```

Edit mask characters:

| Symbol | Meaning |
|--------|---------|
| `Z` | Zero-suppress (blank if zero) |
| `9` | Always display digit |
| `,` | Insert comma (suppressed with leading zeros) |
| `.` | Insert decimal point |

The value 1234567 (representing 12,345.67) formats as: `"12,345.67"`

Leading zeros become spaces, commas within suppressed zeros disappear:

| Value | Output |
|-------|--------|
| 1234567 | `12,345.67` |
| 567 | `     5.67` |
| 0 | `     0.00` |

Edit masks are read-only—you can't write to an edited field.

---

## REDEFINES: Multiple Views

Sometimes you need to interpret the same bytes differently. `REDEFINES` creates overlays:

```zing
layout Message bytes: 100 {
    raw: at: 0 as: pic "X(100)".
    header: at: 0 as: pic "X(10)" redefines: raw.
    body: at: 10 as: pic "X(90)" redefines: raw.
}.
```

All three fields reference the same memory:
- `raw` — full 100 bytes
- `header` — first 10 bytes (overlaps raw[0:10])
- `body` — next 90 bytes (overlaps raw[10:100])

Writing to `raw` affects what you read from `header` and `body`. This is intentional—REDEFINES gives you multiple typed views of the same storage.

Common uses:
- Parsing variable-format messages
- Accessing parts of a larger buffer
- Union-like structures (different interpretations of same data)

---

## Byte-Level Access

For fine-grained control, use the byte accessor intrinsics:

```zing
use Core.Hopper.Bytes.

-- Read
let b = readU8At(ref, offset).     -- Single byte
let w = readU16leAt(ref, offset).  -- 16-bit little-endian
let d = readU32leAt(ref, offset).  -- 32-bit little-endian

-- Write  
writeU8At(ref, offset, value).
writeU16leAt(ref, offset, value).
writeU32leAt(ref, offset, value).
```

These bypass layout fields entirely—you're working with raw arena memory. Use them for:
- Custom binary protocols
- Performance-critical inner loops
- Layouts that don't fit PIC patterns

All accessors return `Result` for bounds safety.

---

## Memory Model

The Hopper arena is a fixed block of memory divided into two regions:

**Arena Storage** (20,000 bytes default)
- Contiguous byte buffer
- Allocations carved sequentially from the front
- Never freed individually (arena-style allocation)

**Reference Table** (64 slots default)
- Maps HopperRef → (offset, size, layout_id)
- Each allocation consumes one slot
- Slots are never reclaimed during execution

This model prioritizes simplicity and predictability:
- No fragmentation (allocations are contiguous)
- No free/realloc (you allocate, you keep it)
- Constant-time lookup (ref → slot → memory)

When you hit limits, allocation fails with `#hopper_oom`. Plan your arena size based on maximum expected records.

---

## Practical Example: Invoice Processing

Here's a complete example processing line items:

```zing
layout LineItem bytes: 40 {
    sku: at: 0 as: pic "X(10)".
    description: at: 10 as: pic "X(20)".
    quantity: at: 30 as: pic "9(4)" usage: #COMP.
    unit_price: at: 34 as: pic "S9(5)V99" usage: #COMP3.
}.

fn process_items(data: Bytes) => Result[i32, Err] {
    let total = 0.
    let offset = 0.
    
    loop {
        if offset >= length(data) then {
            break.
        }.
        
        Result if Hopper record: #LineItem {
            ok item => {
                -- Parse raw data into fields
                let chunk = slice(data, offset, 40).
                Result if sku(item, slice(chunk, 0, 10)) {
                    ok _ => {
                        Result if description(item, slice(chunk, 10, 20)) {
                            ok _ => {
                                -- Read quantity and price, compute line total
                                Result if quantity(item) {
                                    ok qty => {
                                        Result if unit_price(item) {
                                            ok price => {
                                                total = total + (qty * price).
                                            }
                                            fail e => return fail(e).
                                        }.
                                    }
                                    fail e => return fail(e).
                                }.
                            }
                            fail e => return fail(e).
                        }.
                    }
                    fail e => return fail(e).
                }.
            }
            fail e => return fail(e).
        }.
        
        offset = offset + 40.
    }.
    
    ok(total).
}.
```

---

## Error Handling

Hopper operations return `Result[T, Err]` to signal failures:

| Error Symbol | Cause |
|--------------|-------|
| `#hopper_oom` | Arena full or reference slots exhausted |
| `#bounds` | Field access outside record bounds |
| `#pic_invalid` | Invalid data for PIC format (e.g., non-digit in numeric) |

Handle these explicitly:

```zing
Result if Hopper record: #MyLayout {
    ok ref => {
        -- proceed
    }
    fail e => {
        match trace(e) {
            #hopper_oom => {
                -- Log, retry with smaller data, or propagate
            }
            _ => return fail(e).
        }.
    }
}.
```

---

## Design Notes

### Why COBOL-Style Layouts?

COBOL's PIC system isn't just legacy—it's battle-tested infrastructure for:
- **Exact representation** — No floating-point surprises
- **Wire compatibility** — Matches external system formats
- **Self-documenting** — Layout is the documentation

Modern alternatives (protobuf, JSON) optimize for flexibility. PIC optimizes for precision and predictability.

### Arena vs. Heap

Traditional heap allocation offers flexibility (allocate anything, free anytime) at the cost of complexity (fragmentation, GC pauses, use-after-free bugs).

Arena allocation trades flexibility for guarantees:
- All allocations freed together (at program end or arena reset)
- No fragmentation within the arena
- No dangling pointers (refs validated on each access)

For batch processing, request handling, or any bounded-lifetime workload, arenas are simpler and faster.

### The Result Discipline

Every fallible Hopper operation returns `Result`. This is intentional friction—you cannot accidentally ignore:
- Out-of-memory conditions
- Bounds violations
- Format errors

The compiler forces you to handle each case. Verbose? Yes. Crash-proof? Also yes.

---

## Quick Reference

### Types

| Type | Description |
|------|-------------|
| `HopperRef` | Handle to arena-allocated record (i32) |

### Core Operations

| Operation | Signature | Notes |
|-----------|-----------|-------|
| `Hopper record:` | `#Layout → Result[HopperRef, Err]` | Allocate new record |

### Layout Syntax

```zing
layout Name bytes: SIZE {
    field: at: OFFSET as: pic "PATTERN" [usage: #MODE] [redefines: other].
}.
```

### PIC Patterns

| Pattern | Type | Storage |
|---------|------|---------|
| `9(n)` | n-digit unsigned | n bytes (DISPLAY) or packed |
| `S9(n)` | n-digit signed | n+1 bytes (DISPLAY) or packed |
| `9(n)V9(m)` | n.m fixed-point | n+m bytes |
| `X(n)` | n characters | n bytes |
| `ZZ..9` | Zero-suppressed | Formatted output |

### Usage Modes

| Mode | Storage | Use Case |
|------|---------|----------|
| `#DISPLAY` | ASCII digits | Text protocols, debugging |
| `#COMP3` | Packed BCD | Financial data, mainframe compat |
| `#COMP` | Binary i32 | Performance, binary protocols |

### Byte Accessors

| Function | Description |
|----------|-------------|
| `readU8At(ref, off)` | Read byte at offset |
| `readU16leAt(ref, off)` | Read 16-bit LE word |
| `readU32leAt(ref, off)` | Read 32-bit LE dword |
| `writeU8At(ref, off, v)` | Write byte |
| `writeU16leAt(ref, off, v)` | Write 16-bit LE |
| `writeU32leAt(ref, off, v)` | Write 32-bit LE |

---

## See Also

- [result.md](result.md) — Error handling patterns
- [error.md](error.md) — Error construction
- [Core.Hopper.Bytes](hopper_bytes.zing) — Byte-level access intrinsics
