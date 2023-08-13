use std::ffi::{c_char, c_int};

/// This should be called from the Dolphin LogManager initialization to ensure that
/// all logging needs on the Rust side are configured appropriately.
///
/// For more information, consult `dolphin_logger::init`.
///
/// Note that `logger_fn` cannot be type-aliased here, otherwise cbindgen will
/// mess up the header output. That said, the function type represents:
///
/// ```
/// void Log(level, log_type, msg);
/// ```
#[no_mangle]
pub extern "C" fn slprs_logging_init(logger_fn: unsafe extern "C" fn(c_int, c_int, *const c_char)) {
    dolphin_integrations::ffi::logger::init(logger_fn);
}

/// Registers a log container, which mirrors a Dolphin `LogContainer` (`RustLogContainer`).
///
/// See `dolphin_logger::register_container` for more information.
#[no_mangle]
pub extern "C" fn slprs_logging_register_container(
    kind: *const c_char,
    log_type: c_int,
    is_enabled: bool,
    default_log_level: c_int,
) {
    dolphin_integrations::ffi::logger::register_container(kind, log_type, is_enabled, default_log_level);
}

/// Updates the configuration for a registered logging container.
///
/// For more information, see `dolphin_logger::update_container`.
#[no_mangle]
pub extern "C" fn slprs_logging_update_container(kind: *const c_char, enabled: bool, level: c_int) {
    dolphin_integrations::ffi::logger::update_container(kind, enabled, level);
}

/// Updates the configuration for registered logging container on mainline
/// 
/// For more information, see `dolphin_logger::update_container`.
#[no_mangle]
pub extern "C" fn slprs_mainline_logging_update_log_level(level: c_int) {
    dolphin_integrations::ffi::logger::mainline_update_log_level(level);
}
