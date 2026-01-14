# FACTS

1) Edit-mask semantics (DISPLAY formatting)

Scope (MVP): Edit masks are output-only formatting for numeric DISPLAY fields. They never accept input.

Supported mask alphabet (MUST)
	•	9 — mandatory digit
	•	Z — zero-suppress digit (leading only)
	•	, — grouping separator (literal comma, conditional)
	•	. — literal decimal point
	•	+ — plus sign position (optional; see signed rules)
	•	- — minus sign position (optional; see signed rules)
	•	space (0x20) — literal space
	•	any other byte → HOPPER_E_PIC_INVALID (reject mask)

Output length (MUST)
	•	Formatting always produces exactly mask_len bytes, or fails.
	•	Output bytes are pure ASCII. No locale. Ever.

Digit source (MUST)
	•	Formatting consumes exactly digits decimal digits (from the stored numeric value), left padded with zeros as needed.
	•	If the stored value cannot be represented in digits digits → HOPPER_E_OVERFLOW.

Z suppression (MUST)
	•	Z suppresses leading zeros only.
	•	Once a non-zero digit has been emitted, all subsequent Z behave like 9.
	•	If all digits are zero, the last digit position (rightmost digit placeholder) must emit '0' even if it is Z. (Canonical COBOL-ish behavior; avoids “blank number”.)

Comma , behavior (MUST)
	•	Commas are conditional:
If any digit to the left has been emitted as non-space (i.e. not suppressed), emit ','. Otherwise emit space ' '.
	•	This keeps grouping deterministic and matches the “suppression eats commas” expectation.

Decimal point . (MUST)
	•	Decimal point is always literal '.' (not conditional).
	•	But the spec must define where decimals come from:
	•	The value is an integer scaled by scale (implied V).
	•	Formatting does not “move digits”; it just prints digits into placeholders.
	•	Therefore: the edit mask is responsible for placing . at the desired location.

Failure conditions (MUST)
	•	If the mask contains more digit placeholders (9 or Z) than digits → HOPPER_E_PIC_INVALID
	•	If it contains fewer digit placeholders than digits → HOPPER_E_PIC_INVALID
	•	If sign is required but no sign position exists (see below) → HOPPER_E_PIC_INVALID

This gives you a fully deterministic, testable rule set.

⸻

2) DISPLAY signed fields (leading sign only, MVP)

Freeze this hard:

Signed DISPLAY storage (MUST)
	•	Signed DISPLAY uses a leading sign byte:
	•	'+' (0x2B) for non-negative
	•	'-' (0x2D) for negative
	•	Followed by exactly digits ASCII digits ('0'..'9').
	•	Total storage bytes = digits + 1.

No overpunch / trailing sign (MUST NOT, for now)
	•	Overpunch (COBOL zoned decimal) is explicitly out of scope for Stage 1.
	•	Trailing sign is out of scope.

This keeps the runtime small and makes cross-language use trivial.

⸻

3) COMP endianness (little-endian, everywhere)

Yes: COMP is always little-endian in memory. (MUST)

Rationale (spec-worthy):
	•	Determinism across hosts and consistent C ABI interop.
	•	Any big-endian system must byte-swap at the boundary (explicit cost, explicit behavior).

So:
	•	COMP reads/writes i16/i32 using LE encoding.
	•	Field size determines width (2 or 4 bytes). Anything else → HOPPER_E_BAD_FIELD.

⸻

4) COMP-3 sign nibble set (C/D/F accepted)

Yes: freeze it:

Accepted sign nibbles (MUST)
	•	0xC → positive
	•	0xD → negative
	•	0xF → unsigned / treated as positive if field is signed, allowed either way

All other sign nibbles → HOPPER_E_PIC_INVALID

This is the sweet spot: compatible with real-world mainframe data and still strict.

⸻

5) Final “go ahead” set of assumptions

If you implement with:
	•	Edit mask rules above
	•	DISPLAY signed = leading +/-
	•	COMP = LE everywhere
	•	COMP-3 accepts C/D/F only

…then yes, proceed. Those are exactly the kind of early freezes that prevent churn later.

⸻

Tiny addendum (one more freeze point that will save pain)

Scaling rule for numeric APIs:
	•	Hopper’s numeric getters/setters operate on the scaled integer.
	•	Example: PIC 9(5)V99 stores “123.45” as 12345.
	•	Therefore scale affects validation (range), but not the external type.

This matches your Zing story and keeps Hopper BigInt-agnostic.

