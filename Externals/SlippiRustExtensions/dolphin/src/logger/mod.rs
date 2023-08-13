//! This library provides a tracing subscriber configuration that works with the
//! Dolphin logging setup.
//!
//! It essentially maps the concept of a `LogContainer` over to Rust, and provides
//! hooks to forward state change calls in. On top of that, this module contains
//! a custom `tracing_subscriber::Layer` that will pass logs back to Dolphin.
//!
//! Ultimately this should mean no log fragmentation or confusion.

use std::ffi::{c_char, c_int, CStr};
use std::sync::{Arc, Once, OnceLock, RwLock};

use tracing::Level;
use tracing_subscriber::prelude::*;

mod layer;
use layer::{convert_dolphin_log_level_to_tracing_level, DolphinLoggerLayer};

/// A type that mirrors a function over on the C++ side; because the library exists as
/// a dylib, it can't depend on any functions from the host application - but we _can_
/// pass in a hook/callback fn.
///
/// This should correspond to:
///
/// ``` notest
/// void LogFn(level, log_type, msg);
/// ```
pub(crate) type ForeignLoggerFn = unsafe extern "C" fn(c_int, c_int, *const c_char);

/// A marker for where logs should be routed to.
///
/// Rust enum variants can't be strings, but we want to be able to pass an
/// enum to the tracing macro `target` field - which requires a static str.
///
/// Thus we'll fake things a bit and just expose a module that keys things
/// accordingly. The syntax will be the same as if using an enum.
///
/// If you want to add a new logger type, you will need to add a new value here
/// and create a corresponding `LogContainer` on the Dolphin side with the corresponding
/// tag. The rest should "just work".
#[allow(non_snake_case)]
#[allow(non_upper_case_globals)]
pub mod Log {
    /// Used for dumping logs from dependencies that we may need to inspect.
    /// This may also get logs that are not properly tagged! If that happens, you need
    /// to fix your logging calls. :)
    pub const DEPENDENCIES: &'static str = "SLIPPI_RUST_DEPENDENCIES";

    /// The default target for EXI tracing.
    pub const EXI: &'static str = "SLIPPI_RUST_EXI";

    /// Logs for `SlippiGameReporter`.
    pub const GameReporter: &'static str = "SLIPPI_RUST_GAME_REPORTER";

    /// Can be used to segment Jukebox logs.
    pub const Jukebox: &'static str = "SLIPPI_RUST_JUKEBOX";
}

/// Represents a `LogContainer` on the Dolphin side.
#[derive(Debug)]
struct LogContainer {
    kind: String,
    log_type: c_int,
    is_enabled: bool,
    level: Level,
}

/// A global stack of `LogContainers`.
///
/// All logger registrations (which require `write`) should happen up-front due to how
/// Dolphin itself works. RwLock here should provide us parallel reader access after.
static LOG_CONTAINERS: OnceLock<Arc<RwLock<Vec<LogContainer>>>> = OnceLock::new();

/// This should be called from the Dolphin LogManager initialization to ensure that
/// all logging needs on the Rust side are configured appropriately.
///
/// *Usually* you do not want a library installing a global logger, however our use case is
/// not so standard: this library does in a sense act as an application due to the way it's
/// called into, and we *want* a global subscriber.
pub fn init(logger_fn: ForeignLoggerFn) {
    let _containers = LOG_CONTAINERS.get_or_init(|| Arc::new(RwLock::new(Vec::new())));

    // A guard so that we can't double-init logging layers.
    static LOGGER: Once = Once::new();

    // We don't use `try_init` here because we do want to
    // know if something else, somehow, registered before us.
    LOGGER.call_once(|| {
        // We do this so that full backtrace's are emitted on any crashes.
        std::env::set_var("RUST_BACKTRACE", "full");

        tracing_subscriber::registry().with(DolphinLoggerLayer::new(logger_fn)).init();
    });
}

/// Registers a log container, which mirrors a Dolphin `LogContainer`.
///
/// This enables passing a configured log level and/or enabled status across the boundary from
/// Dolphin to our tracing subscriber setup. This is important as we want to short-circuit any
/// allocations during log handling that aren't necessary (e.g if a log is outright disabled).
pub fn register_container(kind: *const c_char, log_type: c_int, is_enabled: bool, default_log_level: c_int) {
    // We control the other end of the registration flow, so we can ensure this ptr's valid UTF-8.
    let c_kind_str = unsafe { CStr::from_ptr(kind) };

    let kind = c_kind_str
        .to_str()
        .expect("[dolphin_logger::register_container]: Failed to convert kind c_char to str")
        .to_string();

    let containers = LOG_CONTAINERS
        .get()
        .expect("[dolphin_logger::register_container]: Attempting to get `LOG_CONTAINERS` before init");

    let mut writer = containers
        .write()
        .expect("[dolphin_logger::register_container]: Unable to acquire write lock on `LOG_CONTAINERS`?");

    (*writer).push(LogContainer {
        kind,
        log_type,
        is_enabled,
        level: convert_dolphin_log_level_to_tracing_level(default_log_level),
    });
}

/// Sets a particular log container to a new enabled state. When a log container is in a disabled
/// state, no allocations will happen behind the scenes for any logging period.
pub fn update_container(kind: *const c_char, enabled: bool, level: c_int) {
    // We control the other end of the registration flow, so we can ensure this ptr's valid UTF-8.
    let c_kind_str = unsafe { CStr::from_ptr(kind) };

    let kind = c_kind_str
        .to_str()
        .expect("[dolphin_logger::update_container]: Failed to convert kind c_char to str");

    let containers = LOG_CONTAINERS
        .get()
        .expect("[dolphin_logger::update_container]: Attempting to get `LOG_CONTAINERS` before init");

    let mut writer = containers
        .write()
        .expect("[dolphin_logger::update_container]: Unable to acquire write lock on `LOG_CONTAINERS`?");

    for container in (*writer).iter_mut() {
        if container.kind == kind {
            container.is_enabled = enabled;
            container.level = convert_dolphin_log_level_to_tracing_level(level);
            break;
        }
    }
}

/// Mainline doesn't have levels per container, but we'll keep it that way to avoid diverging the codebase.
/// Once Ishii dies we should remove this and refactor all this.
pub fn mainline_update_log_level(level: c_int) {
    let containers = LOG_CONTAINERS
        .get()
        .expect("[dolphin_logger::mainline_update_log_level]: Attempting to get `LOG_CONTAINERS` before init");

    let mut writer = containers
        .write()
        .expect("[dolphin_logger::mainline_update_log_level]: Unable to acquire write lock on `LOG_CONTAINERS`?");

    for container in (*writer).iter_mut() {
        container.level = convert_dolphin_log_level_to_tracing_level(level);
    }
}
