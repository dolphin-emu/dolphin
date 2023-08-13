//! Implements potential desync ISO detection. The function(s) in this module should typically
//! be called from a background thread due to processing time.

use std::fs::File;
use std::sync::{Arc, Mutex};

use chksum::prelude::*;

use dolphin_integrations::{Color, Dolphin, Duration, Log};

/// ISO hashes that are known to cause problems. We alert the player
/// if we detect that they're running one.
const KNOWN_DESYNC_ISOS: [&'static str; 4] = [
    "23d6baef06bd65989585096915da20f2",
    "27a5668769a54cd3515af47b8d9982f3",
    "5805fa9f1407aedc8804d0472346fc5f",
    "9bb3e275e77bb1a160276f2330f93931",
];

/// Computes an MD5 hash of the ISO at `iso_path` and writes it back to the value
/// behind `iso_hash`.
///
/// This function is currently more defensive than it probably needs to be, but while
/// we move things into Rust I'd like to reduce the chances of anything panic'ing back
/// into C++ since that can produce undefined behavior. This just handles every possible
/// failure gracefully - however seemingly rare - and simply logs the error.
pub fn run(iso_hash: Arc<Mutex<String>>, iso_path: String) {
    let digest = match File::open(&iso_path) {
        Ok(mut file) => match file.chksum(HashAlgorithm::MD5) {
            Ok(digest) => digest,

            Err(error) => {
                tracing::error!(target: Log::GameReporter, ?error, "Unable to produce ISO MD5 Hash");

                return;
            },
        },

        Err(error) => {
            tracing::error!(target: Log::GameReporter, ?error, "Unable to open ISO for MD5 hashing");

            return;
        },
    };

    let hash = format!("{:x}", digest);

    if !KNOWN_DESYNC_ISOS.contains(&hash.as_str()) {
        tracing::info!(target: Log::GameReporter, iso_md5_hash = ?hash);
    } else {
        // Dump it into the logs as well in case we're ever looking at a user's
        // logs - may end up being faster than trying to debug with them.
        tracing::warn!(
            target: Log::GameReporter,
            iso_md5_hash = ?hash,
            "Potential desync ISO detected"
        );

        // This has more line breaks in the C++ version and I frankly do not have the context as to
        // why they were there - some weird string parsing issue...?
        //
        // Settle on 2 (4 before) as a middle ground I guess.
        Dolphin::add_osd_message(
            Color::Red,
            Duration::Custom(20000),
            "\n\nCAUTION: You are using an ISO that is known to cause desyncs",
        );
    }

    match iso_hash.lock() {
        Ok(mut iso_hash) => {
            *iso_hash = hash;
        },

        Err(error) => {
            tracing::error!(target: Log::GameReporter, ?error, "Unable to lock iso_hash");
        },
    };
}
