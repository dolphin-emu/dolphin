//! This module implements a custom `tracing_subscriber::Layer` that facilitates
//! routing logs through the Dolphin log viewer.

use std::ffi::CString;
use std::fmt::Write;
use std::os::raw::{c_char, c_int};

use time::OffsetDateTime;
use tracing::{Level, Metadata};
use tracing_subscriber::Layer;

use super::{ForeignLoggerFn, Log, LOG_CONTAINERS};

/// Corresponds to Dolphin's `LogTypes::LOG_LEVELS::LNOTICE` value.
#[allow(dead_code)]
const LOG_LEVEL_NOTICE: c_int = 1;

/// Corresponds to Dolphin's `LogTypes::LOG_LEVELS::LERROR` value.
const LOG_LEVEL_ERROR: c_int = 2;

/// Corresponds to Dolphin's `LogTypes::LOG_LEVELS::LWARNING` value.
const LOG_LEVEL_WARNING: c_int = 3;

/// Corresponds to Dolphin's `LogTypes::LOG_LEVELS::LINFO` value.
const LOG_LEVEL_INFO: c_int = 4;

/// Corresponds to Dolphin's `LogTypes::LOG_LEVELS::LDEBUG` value.
const LOG_LEVEL_DEBUG: c_int = 5;

/// A helper method for converting Dolphin's levels to a tracing::Level.
///
/// Currently there's a bit of a mismatch, as `NOTICE` from Dolphin isn't
/// really covered here...
pub fn convert_dolphin_log_level_to_tracing_level(level: c_int) -> Level {
    match level {
        LOG_LEVEL_ERROR => Level::ERROR,
        LOG_LEVEL_WARNING => Level::WARN,
        LOG_LEVEL_INFO => Level::INFO,
        _ => Level::DEBUG,
    }
}

/// A custom tracing layer that forwards events back into the Dolphin logging infrastructure.
///
/// This implements `tracing_subscriber::Layer` and is the default way to log in this library.
#[derive(Debug)]
pub struct DolphinLoggerLayer {
    logger_fn: ForeignLoggerFn,
}

impl DolphinLoggerLayer {
    /// Creates and returns a new logger layer.
    pub fn new(logger_fn: ForeignLoggerFn) -> Self {
        Self { logger_fn }
    }
}

impl<S> Layer<S> for DolphinLoggerLayer
where
    S: tracing::Subscriber,
{
    /// Unpacks a tracing event and routes it to the appropriate Dolphin log handler.
    ///
    /// At the moment, this is somewhat "dumb" and may allocate more than we want to. Consider
    /// it a hook for safely improving performance later. ;P
    fn on_event(&self, event: &tracing::Event<'_>, _ctx: tracing_subscriber::layer::Context<'_, S>) {
        let metadata = event.metadata();
        let target = metadata.target();

        let log_containers = LOG_CONTAINERS
            .get()
            .expect("[DolphinLoggerLayer::on_event]: Unable to acquire `LOG_CONTAINERS`?");

        let reader = log_containers
            .read()
            .expect("[DolphinLoggerLayer::on_event]: Unable to acquire readlock on `LOG_CONTAINERS`?");

        let mut log_container = reader.iter().find(|container| container.kind == target);

        // If we can't find the requested container, then go ahead and see if we can dump it
        // into the dependencies container. This also allows us to grab any logs that *aren't*
        // tagged that may be of interest (i.e, from dependencies - hence the container name).
        if log_container.is_none() {
            log_container = reader.iter().find(|container| container.kind == Log::DEPENDENCIES);
        }

        if log_container.is_none() {
            // We want to still dump errors to the console if no log handler is set at all,
            // otherwise debugging is a nightmare (i.e, we want to surface event flow if a
            // logger initialization is mis-called somewhere).
            eprintln!("No logger handler found for target: {}", target);
            return;
        }

        let container = log_container.unwrap();
        let level = *metadata.level();

        // In tracing, ERROR is the *lowest* - so we essentially just want to make sure that we're
        // at a level *above* the container level before allocating a log message.
        if !container.is_enabled {
            // @TODO: We want to avoid any allocations if the log level
            // is not appropriate, but there's a mismatch between how Dolphin prioritizes log
            // levels and how tracing does it.
            //
            // || container.level > level {
            return;
        }

        let log_level = match level {
            Level::INFO => LOG_LEVEL_INFO,
            Level::WARN => LOG_LEVEL_WARNING,
            Level::ERROR => LOG_LEVEL_ERROR,
            Level::DEBUG | Level::TRACE => LOG_LEVEL_DEBUG,
        };

        let mut visitor = DolphinLoggerVisitor::new(metadata, target);
        event.record(&mut visitor);

        match CString::new(visitor.finish()) {
            Ok(c_str_msg) => {
                // A note on ownership: the Dolphin logger cannot modify the contents of
                // the passed over c_str, nor should it attempt to free anything. Rust owns
                // this and will handle any cleanup after the logger_fn has dispatched.
                unsafe {
                    (self.logger_fn)(log_level, container.log_type, c_str_msg.as_ptr() as *const c_char);
                }
            },

            // This should never happen, but on the off chance it does, I guess
            // just dump it to stderr?
            Err(e) => {
                eprintln!("Failed to convert info msg to CString: {:?}", e);
            },
        }
    }
}

/// Implements a visitor that builds a log message for the logger functionality in Dolphin
/// to consume. This currently builds a String internally that can be passed over to
/// the Dolphin side.
#[derive(Debug)]
struct DolphinLoggerVisitor(String);

impl DolphinLoggerVisitor {
    /// Creates and returns a new `DolphinLoggerVisitor`.
    ///
    /// This performs some initial work up front to build a log that is similar to
    /// what Dolphin would construct; we're doing it here to avoid extra allocations
    /// that would occur on the Dolphin logging side otherwise.
    pub fn new(metadata: &Metadata, log_type: &str) -> Self {
        let file = metadata.file().unwrap_or("");
        let line = metadata.line().unwrap_or(0);
        let level = metadata.level();

        // Dolphin logs in the format of {Minutes}:{Seconds}:{Milliseconds}.
        let time = OffsetDateTime::now_local().unwrap_or_else(|e| {
            eprintln!("[dolphin_logger/layer.rs] Failed to get local time: {:?}", e);

            // This will only happen if, for whatever reason, the timezone offset
            // on the current system cannot be determined. Frankly there's bigger issues
            // than logging if that's the case.
            OffsetDateTime::now_utc()
        });

        let mins = time.minute();
        let secs = time.second();
        let millsecs = time.millisecond();

        // We want 0-padded mins/secs, but we don't need the entire formatting infra
        // that time would use - and this is simple enough to just do in a few lines.
        let mp = match mins < 10 {
            true => "0",
            false => "",
        };

        let sp = match secs < 10 {
            true => "0",
            false => "",
        };

        Self(format!(
            "{mp}{mins}:{sp}{secs}:{millsecs} {file}:{line} {level}[{log_type}]: "
        ))
    }

    /// The Dolphin log window needs a newline attached to the end, so we just write one
    /// as a finishing method.
    pub fn finish(mut self) -> String {
        if let Err(e) = write!(&mut self.0, "\n") {
            eprintln!("Failed to finish logging string: {:?}", e);
        }

        self.0
    }
}

impl tracing::field::Visit for DolphinLoggerVisitor {
    fn record_f64(&mut self, field: &tracing::field::Field, value: f64) {
        if let Err(e) = write!(&mut self.0, "{}={} ", field.name(), value) {
            eprintln!("Failed to record_error: {:?}", e);
        }
    }

    fn record_i64(&mut self, field: &tracing::field::Field, value: i64) {
        if let Err(e) = write!(&mut self.0, "{}={} ", field.name(), value) {
            eprintln!("Failed to record_error: {:?}", e);
        }
    }

    fn record_u64(&mut self, field: &tracing::field::Field, value: u64) {
        if let Err(e) = write!(&mut self.0, "{}={} ", field.name(), value) {
            eprintln!("Failed to record_error: {:?}", e);
        }
    }

    fn record_i128(&mut self, field: &tracing::field::Field, value: i128) {
        if let Err(e) = write!(&mut self.0, "{}={} ", field.name(), value) {
            eprintln!("Failed to record_error: {:?}", e);
        }
    }

    fn record_u128(&mut self, field: &tracing::field::Field, value: u128) {
        if let Err(e) = write!(&mut self.0, "{}={} ", field.name(), value) {
            eprintln!("Failed to record_error: {:?}", e);
        }
    }

    fn record_bool(&mut self, field: &tracing::field::Field, value: bool) {
        if let Err(e) = write!(&mut self.0, "{}={} ", field.name(), value) {
            eprintln!("Failed to record_bool: {:?}", e);
        }
    }

    fn record_str(&mut self, field: &tracing::field::Field, value: &str) {
        if let Err(e) = write!(&mut self.0, "{}={} ", field.name(), value) {
            eprintln!("Failed to record_str: {:?}", e);
        }
    }

    fn record_error(&mut self, field: &tracing::field::Field, value: &(dyn std::error::Error + 'static)) {
        if let Err(e) = write!(&mut self.0, "{}={} ", field.name(), value) {
            eprintln!("Failed to record_error: {:?}", e);
        }
    }

    fn record_debug(&mut self, field: &tracing::field::Field, value: &dyn std::fmt::Debug) {
        if let Err(e) = write!(&mut self.0, "{}={:?} ", field.name(), value) {
            eprintln!("Failed to record_debug: {:?}", e);
        }
    }
}
