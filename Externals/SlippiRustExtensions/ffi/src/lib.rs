//! This library is the core interface for the Rust side of things, and consists
//! predominantly of C FFI bridging functions that can be called from the Dolphin
//! side of things.
//!
//! This library auto-generates C headers on build, and Slippi Dolphin is pre-configured
//! to locate these headers and link the entire dylib.

use std::ffi::{c_char, CStr};

pub mod exi;
pub mod game_reporter;
pub mod logger;

/// A small helper method for moving in and out of our known types.
pub(crate) fn set<T, F>(instance_ptr: usize, handler: F)
where
    F: FnOnce(&mut T),
{
    // This entire method could possibly be a macro but I'm way too tired
    // to deal with that syntax right now.

    // Coerce the instance from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `instance_ptr` is only owned
    // by us, and is created/destroyed with the corresponding lifetimes.
    let mut instance = unsafe { Box::from_raw(instance_ptr as *mut T) };

    handler(&mut instance);

    // Fall back into a raw pointer so Rust doesn't obliterate the object.
    let _leak = Box::into_raw(instance);
}

/// A helper function for converting c str types to Rust ones with
/// some optional args for aiding in debugging should this ever be a problem.
///
/// This will panic if the strings being passed over cannot be converted, as
/// we need the game reporter to be able to run without question. It's also just
/// far less verbose than doing this all over the place.
pub(crate) fn c_str_to_string(string: *const c_char, fn_label: &str, err_label: &str) -> String {
    // This is theoretically safe as we control the strings being passed from
    // the C++ side, and can mostly guarantee that we know what we're getting.
    let slice = unsafe { CStr::from_ptr(string) };

    match slice.to_str() {
        Ok(s) => s.to_string(),

        Err(e) => {
            tracing::error!(
                error = ?e,
                "[{}] Failed to bridge {}, will panic",
                fn_label,
                err_label
            );

            panic!("Unable to bridge necessary type, panicing");
        },
    }
}
