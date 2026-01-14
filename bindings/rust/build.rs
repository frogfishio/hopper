// SPDX-FileCopyrightText: 2026 Frogfish
// SPDX-License-Identifier: Apache-2.0
// Author: Alexander Croft <alex@frogfish.io>

use std::path::PathBuf;

fn main() {
    let lib = pkg_config::Config::new()
        .atleast_version("1")
        .probe("hopper")
        .expect("pkg-config hopper");

    println!("cargo:rerun-if-changed=wrapper.h");

    let mut builder = bindgen::Builder::default().header("wrapper.h");
    builder = builder
        .allowlist_type("hopper_.*")
        .allowlist_function("hopper_.*")
        .allowlist_var("HOPPER_.*");

    for path in lib.include_paths {
        builder = builder.clang_arg(format!("-I{}", path.display()));
    }

    let bindings = builder
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(std::env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
