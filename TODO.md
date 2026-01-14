# TODO

State: core ABI and runtime implemented (header, hopper.c, pic.c, tests, Makefile). Next steps to reach a publishable universal library:

1) Build & packaging
   - Add shared library targets (libhopper.so/.dylib) alongside libhopper.a.
   - Add install rules for headers + libs and a pkg-config file (hopper.pc).
   - Optional: export symbols with visibility annotations for shared builds.

2) Docs
   - Write doc/abi.md describing ABI versioning, struct layouts, error codes, endianness, and sizing guidance (`hopper_ref_entry_sizeof`).
   - Add a conformance checklist (what behaviors are covered by tests and what must hold).

3) Testing
   - Expand edit-mask coverage (more comma/decimal patterns, zero suppression edge cases).
   - Add scale-mismatch and COMP/COMP-3 formatting tests if formatting for non-DISPLAY is added.
   - Create `make check` (or similar) that runs the full test suite.
   - Set up CI (e.g., GitHub Actions) to build + run tests on at least one platform.

4) Integration/bindings
   - Provide a minimal catalog schema/format description so non-Zing toolchains can supply layout metadata.
   - Add thin bindings (or examples) for other languages if desired (C++ wrapper, Zig/Rust/Python FFI stubs).

5) Zing integration
   - Switch Zing codegen to call the Hopper ABI instead of inlined Hopper code, and add end-to-end Zing tests using the shared library.

6) Performance/QA (optional but good)
   - Add microbenchmarks for alloc/read/write paths.
   - Run valgrind/asan/ubsan passes in CI to enforce determinism and no hidden allocations.
