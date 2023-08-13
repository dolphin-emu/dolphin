//! Implements types and some helpers used in rendering messages via the On-Screen-Display
//! functionality in Dolphin.

use std::ffi::c_char;

use std::sync::OnceLock;

/// A type that mirrors.
pub type AddOSDMessageFn = unsafe extern "C" fn(*const c_char, u32, u32);

/// Global holder for our message handler fn.
pub(crate) static MESSAGE_HOOK: OnceLock<AddOSDMessageFn> = OnceLock::new();

/// Sets the global hook for sending an On-Screen-Display message back to Dolphin.
///
/// Once set, this hook cannot be changed - there's no scenario where we expect to
/// ever need to do that anyway.
pub fn set_global_hook(add_osd_message_fn: AddOSDMessageFn) {
    if MESSAGE_HOOK.get().is_none() {
        MESSAGE_HOOK.set(add_osd_message_fn).expect("");
    }
}

/// Represents colors that the On-Screen-Display could render.
#[derive(Clone, Copy, Debug)]
pub enum Color {
    Cyan,
    Green,
    Red,
    Yellow,
    Custom(u32),
}

impl Color {
    /// Returns a `u32` representation of this type.
    pub fn to_u32(&self) -> u32 {
        match self {
            Self::Cyan => 0xFF00FFFF,
            Self::Green => 0xFF00FF00,
            Self::Red => 0xFFFF0000,
            Self::Yellow => 0xFFFFFF30,
            Self::Custom(value) => *value,
        }
    }
}

/// Represents the length of time an On-Screen-Display message should stay on screen.
#[derive(Clone, Copy, Debug)]
pub enum Duration {
    Short,
    Normal,
    VeryLong,
    Custom(u32),
}

impl Duration {
    /// Returns a u32 representation of this type.
    pub fn to_u32(&self) -> u32 {
        match self {
            Self::Short => 2000,
            Self::Normal => 5000,
            Self::VeryLong => 10000,
            Self::Custom(value) => *value,
        }
    }
}
