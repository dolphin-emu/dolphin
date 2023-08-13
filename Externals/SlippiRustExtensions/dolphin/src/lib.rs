//! This module implements various shims for calling back into C++ (the Dolphin side of things).
//!
//! Typically you can just import the `Dolphin` struct and use that as a namespace for accessing
//! all functionality at once.

use std::ffi::{c_char, CString};

mod logger;
pub use logger::Log;

mod osd;
pub use osd::{Color, Duration};

/// These types are primarily used by the `ffi` crate to wire up Dolphin functionality,
/// and you shouldn't need to touch them yourself. We're just re-exporting these under
/// a different namespace to avoid cluttering up autocomplete and confusing anyone.
pub mod ffi {
    pub mod osd {
        pub use crate::osd::set_global_hook;
    }

    pub mod logger {
        pub use crate::logger::{init, register_container, update_container, mainline_update_log_level};
    }
}

/// At the moment, this struct holds nothing and is simply a namespace to make importing and
/// accessing functionality less verbose.
///
/// tl;dr it's a namespace.
pub struct Dolphin;

impl Dolphin {
    /// Renders a message in the current game window via the On-Screen-Display.
    ///
    /// This function accepts anything that can be allocated into a `String` (which
    /// includes just passing a `String` itself).
    ///
    /// ```no_run
    /// use dolphin_integrations::{Color, Dolphin, Duration};
    ///
    /// fn stuff() {
    ///     Dolphin::add_osd_message(Color::Cyan, Duration::Short, "Hello there");
    /// }
    /// ```
    pub fn add_osd_message<S>(color: Color, duration: Duration, message: S)
    where
        S: Into<String>,
    {
        let message: String = message.into();

        if let Some(hook_fn) = osd::MESSAGE_HOOK.get() {
            let c_str_msg = match CString::new(message) {
                Ok(c_string) => c_string,

                Err(error) => {
                    tracing::error!(?error, "Unable to allocate OSD String, not rendering");

                    return;
                },
            };

            let color = color.to_u32();
            let duration = duration.to_u32();

            unsafe {
                hook_fn(c_str_msg.as_ptr() as *const c_char, color, duration);
            }
        }
    }
}
