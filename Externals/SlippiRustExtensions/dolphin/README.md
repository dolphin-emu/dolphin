# Dolphin Integrations
This crate implements various hooks and functionality for interacting with Dolphin from the Rust side. Notably, it contains a custom [tracing-subscriber](https://crates.io/crates/tracing-subscriber) that handles shuttling logs through the Dolphin application logging infrastructure. This is important not just for keeping logs contained all in one place, but for aiding in debugging on Windows where console logging is... odd.

## How Logging works
The first thing to understand is that the Slippi Dolphin `LogManager` module has some tweaks to support creating a "Rust-sourced" `LogContainer`.

When the `LogManager` is created, we initialize the Rust logging framework by calling `slprs_logging_init`, passing a function to dispatch logs through from the Rust side.

The `LogManager` has several `LogContainer` instances, generally corresponding to a particular grouping of log types. If a `LogContainer` is flagged as sourcing from the Rust library, then on instantiation it will register itself with the Rust logger by calling `slprs_logging_register_container`. This registration call caches the `LogType` and enabled status of the log so that the Rust side can transparently keep track of things and not have to concern itself with any changing enum variant types.

When a Rust-sourced `LogContainer` is updated - say, a status change to `disabled` - it will forward this change to the Rust side via `slprs_logging_update_container`. If a log container is disabled in Rust, then logs are dropped accordingly with no allocations made anywhere (i.e, nothing should be impacting gameplay).

## Adding a new LogContainer
If you need to add a new log to the Dolphin codebase, you will need to add a few lines to the log definitions on the C++ side. This enables using the Dolphin logs viewer to monitor Rust `tracing` events.

First, head to [`Source/Core/Common/Logging/Log.h`](../../../Source/Core/Common/Logging/Log.h) and define a new `LogTypes::LOG_TYPE` variant.

Next, head to [`Source/Core/Common/Logging/LogManager.cpp`](../../../Source/Core/Common/Logging/LogManager.cpp) and add a new `LogContainer` in `LogManager::LogManager()`. For example, let's say that we created `LogTypes::SLIPPI_RUST_EXI` - we'd now add:

``` c++
// This LogContainer will register with the Rust side under the "SLIPPI_RUST_EXI" target.
m_Log[LogTypes::SLIPPI_RUST_EXI] = new LogContainer(
    "SLIPPI_RUST_EXI",  // Internal identifier, Rust will need to match
    "Slippi EXI (Rust)",  // User-visible log label
    LogTypes::SLIPPI_RUST_EXI,  // The C++ LogTypes variant we created
    true  // Instructs the initializer that this is a Rust-sourced log
);
```

Finally, add an associated `const` declaration with your `LogContainer` internal identifier in `src/lib.rs`:

``` rust
pub mod Log {
    // ...other logs etc
    
    // Our new logger name
    pub const EXI: &'static str = "SLIPPI_RUST_EXI";
}
```

Now your Rust module can specify that it should log to this container via the tracing module:

``` rust
use dolphin_integrations::Log;

fn do_stuff() {
    tracing::info!(target: Log::EXI, "Hello from the Rust side");
}
```

Make sure to _enable_ your log via Dolphin's Log Manager view to see these logs!
