//! This build script simply generates C FFI bindings for the freestanding
//! functions in `lib.rs` and dumps them into a header that the Dolphin
//! project is pre-configured to find.

use std::env;

fn main() {
    let crate_dir = env::var("CARGO_MANIFEST_DIR").unwrap();

    // We disable `enum class` declarations to mirror what Ishiiruka
    // currently does.
    let enum_config = cbindgen::EnumConfig {
        enum_class: false,
        ..Default::default()
    };

    let config = cbindgen::Config {
        enumeration: enum_config,
        ..Default::default()
    };

    cbindgen::Builder::new()
        .with_config(config)
        .with_crate(crate_dir)
        .generate()
        .expect("Unable to generate bindings")
        .write_to_file("includes/SlippiRustExtensions.h");
}
