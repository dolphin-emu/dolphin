//! This could be rewritten down the road, but the goal is a 1:1 port right now,
//! not to rewrite the universe.

use std::ops::Deref;
use std::sync::mpsc::{self, Sender};
use std::thread;

use dolphin_integrations::Log;

mod iso_md5_hasher;

mod queue;
use queue::GameReporterQueue;

mod types;
pub use types::{GameReport, OnlinePlayMode, PlayerReport};

/// Events that we dispatch into the processing thread.
#[derive(Copy, Clone, Debug)]
pub(crate) enum ProcessingEvent {
    ReportAvailable,
    Shutdown,
}

/// The public interface for the game reporter service. This handles managing any
/// necessary background threads and provides hooks for instrumenting the reporting
/// process.
///
/// The inner `GameReporter` is shared between threads and manages field access via
/// internal Mutexes. We supply a channel to the processing thread in order to notify
/// it of new reports to process.
#[derive(Debug)]
pub struct SlippiGameReporter {
    iso_md5_hasher_thread: Option<thread::JoinHandle<()>>,
    processing_thread: Option<thread::JoinHandle<()>>,
    processing_thread_notifier: Sender<ProcessingEvent>,
    queue: GameReporterQueue,
    replay_data: Vec<u8>,
}

impl SlippiGameReporter {
    /// Initializes and returns a new `SlippiGameReporter`.
    ///
    /// This spawns and manages a few background threads to handle things like
    /// report and upload processing, along with checking for troublesome ISOs.
    /// The core logic surrounding reports themselves lives a layer deeper in `GameReporter`.
    ///
    /// Currently, failure to spawn any thread should result in a crash - i.e, if we can't
    /// spawn an OS thread, then there are probably far bigger issues at work here.
    pub fn new(iso_path: String) -> Self {
        let queue = GameReporterQueue::new();

        // This is a thread-safe "one time" setter that the MD5 hasher thread
        // will set when it's done computing.
        let iso_hash_setter = queue.iso_hash.clone();

        let iso_md5_hasher_thread = thread::Builder::new()
            .name("SlippiGameReporterISOHasherThread".into())
            .spawn(move || {
                iso_md5_hasher::run(iso_hash_setter, iso_path);
            })
            .expect("Failed to spawn SlippiGameReporterISOHasherThread.");

        let (sender, receiver) = mpsc::channel();
        let processing_thread_queue_handle = queue.clone();

        let processing_thread = thread::Builder::new()
            .name("SlippiGameReporterProcessingThread".into())
            .spawn(move || {
                queue::run(processing_thread_queue_handle, receiver);
            })
            .expect("Failed to spawn SlippiGameReporterProcessingThread.");

        Self {
            queue,
            replay_data: Vec::new(),
            processing_thread_notifier: sender,
            processing_thread: Some(processing_thread),
            iso_md5_hasher_thread: Some(iso_md5_hasher_thread),
        }
    }

    /// Currently unused.
    pub fn start_new_session(&mut self) {
        // Maybe we could do stuff here? We used to initialize gameIndex but
        // that isn't required anymore
    }

    /// Logs replay data that's passed to it.
    pub fn push_replay_data(&mut self, data: &[u8]) {
        self.replay_data.extend_from_slice(data);
    }

    /// Adds a report for processing and signals to the processing thread that there's
    /// work to be done.
    ///
    /// Note that when a new report is added, we transfer ownership of all current replay data
    /// to the game report itself. By doing this, we avoid needing to have a Mutex controlling
    /// access and pushing replay data as it comes in requires no locking.
    pub fn log_report(&mut self, mut report: GameReport) {
        report.replay_data = std::mem::replace(&mut self.replay_data, Vec::new());
        self.queue.add_report(report);

        if let Err(e) = self.processing_thread_notifier.send(ProcessingEvent::ReportAvailable) {
            tracing::error!(
                target: Log::GameReporter,
                error = ?e,
                "Unable to dispatch ReportAvailable notification"
            );
        }
    }
}

impl Deref for SlippiGameReporter {
    type Target = GameReporterQueue;

    /// Support dereferencing to the inner game reporter. This has a "subclass"-like
    /// effect wherein we don't need to duplicate methods on this layer.
    fn deref(&self) -> &Self::Target {
        &self.queue
    }
}

impl Drop for SlippiGameReporter {
    /// Joins the background threads when we're done, logging if
    /// any errors are encountered.
    fn drop(&mut self) {
        if let Some(processing_thread) = self.processing_thread.take() {
            if let Err(e) = self.processing_thread_notifier.send(ProcessingEvent::Shutdown) {
                tracing::error!(
                    target: Log::GameReporter,
                    error = ?e,
                    "Failed to send shutdown notification to report processing thread, may hang"
                );
            }

            if let Err(e) = processing_thread.join() {
                tracing::error!(
                    target: Log::GameReporter,
                    error = ?e,
                    "Processing thread failure"
                );
            }
        }

        if let Some(iso_md5_hasher_thread) = self.iso_md5_hasher_thread.take() {
            if let Err(e) = iso_md5_hasher_thread.join() {
                tracing::error!(
                    target: Log::GameReporter,
                    error = ?e,
                    "ISO MD5 hasher thread failure"
                );
            }
        }
    }
}
