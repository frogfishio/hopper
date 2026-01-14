# SPDX-FileCopyrightText: 2026 Frogfish
# SPDX-License-Identifier: Apache-2.0
# Author: Alexander Croft <alex@frogfish.io>

# Rust binding sketch

Goal: generate Rust FFI bindings from `hopper.h` and link against `libhopper`.

## Quick steps
1. Ensure Hopper is installed (or built locally) and `pkg-config` can find `hopper`.
2. Add `bindgen` as a build-dependency in `Cargo.toml`.
3. Add a `build.rs` to generate bindings:
```rust
fn main() {
    let lib = pkg_config::Config::new()
        .atleast_version("1")
        .probe("hopper")
        .expect("pkg-config hopper");
    println!("cargo:rerun-if-changed=wrapper.h");
    for path in lib.include_paths {
        println!("cargo:include={}", path.display());
    }
    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        .allowlist_type("hopper_.*")
        .allowlist_function("hopper_.*")
        .allowlist_var("HOPPER_.*")
        .generate()
        .expect("bindgen");
    bindings
        .write_to_file("src/bindings.rs")
        .expect("write bindings");
}
```
4. `wrapper.h` should include Hopper:
```c
#include <hopper.h>
```
5. In `Cargo.toml`, link to hopper:
```toml
[package]
links = "hopper"

[build-dependencies]
bindgen = "0.69"
pkg-config = "0.3"
```

## Usage
- Import the generated `bindings.rs` from your Rust code.
- Mirror the error enum as a Rust enum (`#[repr(i32)]`) if you prefer type safety.
- Remember to allocate arena/ref memory in Rust (e.g., `Vec<u8>`) and pass pointers/lengths matching Hopper’s config.

## Minimal Rust example (unsafe FFI)
See `src/lib.rs` after running `cargo build` (bindgen generates bindings into `OUT_DIR`). The generated API matches `hopper.h`; call `version()` for a safe wrapper around `hopper_version()`.
