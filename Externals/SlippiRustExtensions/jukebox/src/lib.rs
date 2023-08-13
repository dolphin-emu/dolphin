mod scenes;
mod tracks;
mod utils;

use anyhow::{Context, Result};
use dolphin_integrations::Log;
use hps_decode::{hps::Hps, pcm_iterator::PcmIterator};
use process_memory::LocalMember;
use process_memory::Memory;
use rodio::{OutputStream, Sink};
use scenes::scene_ids::*;
use std::convert::TryInto;
use std::ops::ControlFlow::{self, Break, Continue};
use std::sync::mpsc::{channel, Receiver, Sender};
use std::{thread::sleep, time::Duration};
use tracks::TrackId;

use dolphin_integrations::{Color, Dolphin, Duration as OSDDuration};

/// Represents a foreign method from the Dolphin side for grabbing the current volume.
/// Dolphin represents this as a number from 0 - 100; 0 being mute.
pub type ForeignGetVolumeFn = unsafe extern "C" fn() -> std::ffi::c_int;

const THREAD_LOOP_SLEEP_TIME_MS: u64 = 30;
const CHILD_THREAD_COUNT: usize = 2;

/// By default Slippi Jukebox plays music slightly louder than vanilla melee
/// does. This reduces the overall music volume output to 80%. Not totally sure
/// if that's the correct amount, but it sounds about right.
const VOLUME_REDUCTION_MULTIPLIER: f32 = 0.8;

#[derive(Debug, PartialEq)]
struct DolphinGameState {
    in_game: bool,
    in_menus: bool,
    scene_major: u8,
    scene_minor: u8,
    stage_id: u8,
    volume: f32,
    is_paused: bool,
    match_info: u8,
}

impl Default for DolphinGameState {
    fn default() -> Self {
        Self {
            in_game: false,
            in_menus: false,
            scene_major: SCENE_MAIN_MENU,
            scene_minor: 0,
            stage_id: 0,
            volume: 0.0,
            is_paused: false,
            match_info: 0,
        }
    }
}

#[derive(Debug)]
enum MeleeEvent {
    TitleScreenEntered,
    MenuEntered,
    LotteryEntered,
    GameStart(u8), // stage id
    GameEnd,
    RankedStageStrikeEntered,
    VsOnlineOpponent,
    Pause,
    Unpause,
    SetVolume(f32),
    NoOp,
}

#[derive(Debug, Clone)]
enum JukeboxEvent {
    Dropped,
}

#[derive(Debug)]
pub struct Jukebox {
    channel_senders: [Sender<JukeboxEvent>; CHILD_THREAD_COUNT],
}

impl Jukebox {
    /// Returns a Jukebox instance that will immediately spawn two child threads
    /// to try and read game memory and play music. When the returned instance is
    /// dropped, the child threads will terminate and the music will stop.
    pub fn new(m_p_ram: *const u8, iso_path: String, get_dolphin_volume_fn: ForeignGetVolumeFn) -> Result<Self> {
        tracing::info!(target: Log::Jukebox, "Initializing Slippi Jukebox");

        // We are implicitly trusting that these pointers will outlive the jukebox instance
        let m_p_ram = m_p_ram as usize;
        let get_dolphin_volume = move || unsafe { get_dolphin_volume_fn() } as f32 / 100.0;

        // This channel is used for the `JukeboxMessageDispatcher` thread to send
        // messages to the `JukeboxMusicPlayer` thread
        let (melee_event_tx, melee_event_rx) = channel::<MeleeEvent>();

        // These channels allow the jukebox instance to notify both child
        // threads when something important happens. Currently its only purpose
        // is to notify them that the instance is about to be dropped so they
        // should terminate
        let (message_dispatcher_thread_tx, message_dispatcher_thread_rx) = channel::<JukeboxEvent>();
        let (music_thread_tx, music_thread_rx) = channel::<JukeboxEvent>();

        std::thread::Builder::new()
            .name("JukeboxMessageDispatcher".to_string())
            .spawn(move || Self::dispatch_messages(m_p_ram, get_dolphin_volume, message_dispatcher_thread_rx, melee_event_tx))?;

        std::thread::Builder::new()
            .name("JukeboxMusicPlayer".to_string())
            .spawn(move || Self::play_music(m_p_ram, &iso_path, get_dolphin_volume, music_thread_rx, melee_event_rx))?;

        Ok(Self {
            channel_senders: [message_dispatcher_thread_tx, music_thread_tx],
        })
    }

    /// This thread continuously reads select values from game memory as well
    /// as the current `volume` value in the dolphin configuration. If it
    /// notices anything change, it will dispatch a message to the
    /// `JukeboxMusicPlayer` thread.
    fn dispatch_messages(
        m_p_ram: usize,
        get_dolphin_volume: impl Fn() -> f32,
        message_dispatcher_thread_rx: Receiver<JukeboxEvent>,
        melee_event_tx: Sender<MeleeEvent>,
    ) -> Result<()> {
        // Initial "dolphin state" that will get updated over time
        let mut prev_state = DolphinGameState::default();

        loop {
            // Stop the thread if the jukebox instance will be been dropped
            if let Ok(event) = message_dispatcher_thread_rx.try_recv() {
                if matches!(event, JukeboxEvent::Dropped) {
                    return Ok(());
                }
            }

            // Continuously check if the dolphin state has changed
            let state = Self::read_dolphin_game_state(&m_p_ram, get_dolphin_volume())?;

            // If the state has changed,
            if prev_state != state {
                // dispatch a message to the music player thread
                let event = Self::produce_melee_event(&prev_state, &state);
                tracing::info!(target: Log::Jukebox, "{:?}", event);

                melee_event_tx.send(event)?;
                prev_state = state;
            }

            sleep(Duration::from_millis(THREAD_LOOP_SLEEP_TIME_MS));
        }
    }

    /// This thread listens for incoming messages from the
    /// `JukeboxMessageDispatcher` thread and handles music playback
    /// accordingly.
    fn play_music(
        m_p_ram: usize,
        iso_path: &str,
        get_dolphin_volume: impl Fn() -> f32,
        music_thread_rx: Receiver<JukeboxEvent>,
        melee_event_rx: Receiver<MeleeEvent>,
    ) -> Result<()> {
        let mut iso = std::fs::File::open(iso_path)?;

        tracing::info!(target: Log::Jukebox, "Loading track metadata...");
        let tracks = utils::create_track_map(&mut iso).map_err(|e| {
            Dolphin::add_osd_message(
                Color::Red,
                OSDDuration::VeryLong,
                "\nError starting Slippi Jukebox. Your ISO is likely incompatible. Music will not play.",
            );
            tracing::error!(target: Log::Jukebox, error = ?e, "Failed to create track map");
            return e;
        })?;
        tracing::info!(target: Log::Jukebox, "Loaded metadata for {} tracks!", tracks.len());

        let (_stream, stream_handle) = OutputStream::try_default()?;
        let sink = Sink::try_new(&stream_handle)?;

        // The menu track and tournament-mode track are randomly selected
        // one time, and will be used for the rest of the session
        let random_menu_tracks = utils::get_random_menu_tracks();

        // Initial music volume and track id. These values will get
        // updated by the `handle_melee_event` fn whenever a message is
        // received from the other thread.
        let initial_state = Self::read_dolphin_game_state(&m_p_ram, get_dolphin_volume())?;
        let mut volume = initial_state.volume;
        let mut track_id: Option<TrackId> = None;

        loop {
            if let Some(track_id) = track_id {
                // Lookup the current track_id in the `tracks` hashmap,
                // and if it's present, then play it. If not, there will
                // be silence until a new track_id is set
                let track = tracks.get(&track_id);
                if let Some(&(offset, size)) = track {
                    let offset = offset as u64;
                    let size = size as usize;

                    // Parse data from the ISO into pcm samples
                    let hps: Hps = utils::read_from_file(&mut iso, offset, size)?
                        .try_into()
                        .with_context(|| format!("The {size} bytes at offset 0x{offset:x?} could not be decoded into an Hps"))?;

                    let padding_length = hps.channel_count * hps.sample_rate / 4;
                    let audio_source = HpsAudioSource {
                        pcm: hps.into(),
                        padding_length,
                    };

                    // Play song
                    sink.append(audio_source);
                    sink.play();
                    sink.set_volume(volume);
                }
            }

            // Continue to play the song indefinitely while regularly checking
            // for new messages from the `JukeboxMessageDispatcher` thread
            loop {
                // Stop the thread if the jukebox instance will be been dropped
                if let Ok(event) = music_thread_rx.try_recv() {
                    if matches!(event, JukeboxEvent::Dropped) {
                        return Ok(());
                    }
                }

                // When we receive an event, handle it. This can include
                // changing the volume or updating the track and breaking
                // the inner loop such that the next track starts to play
                if let Ok(event) = melee_event_rx.try_recv() {
                    if let Break(_) = Self::handle_melee_event(event, &sink, &mut track_id, &mut volume, &random_menu_tracks) {
                        break;
                    }
                }

                sleep(Duration::from_millis(THREAD_LOOP_SLEEP_TIME_MS));
            }

            sink.stop();
        }
    }

    /// Handle a events received in the audio playback thread, by changing tracks,
    /// adjusting volume etc.
    fn handle_melee_event(
        event: MeleeEvent,
        sink: &Sink,
        track_id: &mut Option<TrackId>,
        volume: &mut f32,
        random_menu_tracks: &(TrackId, TrackId),
    ) -> ControlFlow<()> {
        use self::MeleeEvent::*;

        // TODO:
        // - Intro movie
        //
        // - classic vs screen
        // - classic victory screen
        // - classic game over screen
        // - classic credits
        // - classic "congratulations movie"
        // - Adventure mode field intro music

        let (menu_track, tournament_track) = random_menu_tracks;

        match event {
            TitleScreenEntered | GameEnd => {
                *track_id = None;
            },
            MenuEntered => {
                *track_id = Some(*menu_track);
            },
            LotteryEntered => {
                *track_id = Some(tracks::TrackId::Lottery);
            },
            VsOnlineOpponent => {
                *track_id = Some(tracks::TrackId::VsOpponent);
            },
            RankedStageStrikeEntered => {
                *track_id = Some(*tournament_track);
            },
            GameStart(stage_id) => {
                *track_id = tracks::get_stage_track_id(stage_id);
            },
            Pause => {
                sink.set_volume(*volume * 0.2);
                return Continue(());
            },
            Unpause => {
                sink.set_volume(*volume);
                return Continue(());
            },
            SetVolume(received_volume) => {
                sink.set_volume(received_volume);
                *volume = received_volume;
                return Continue(());
            },
            NoOp => {
                return Continue(());
            },
        };

        Break(())
    }

    /// Given the previous dolphin state and current dolphin state, produce an event
    fn produce_melee_event(prev_state: &DolphinGameState, state: &DolphinGameState) -> MeleeEvent {
        let vs_screen_1 = state.scene_major == SCENE_VS_ONLINE
            && prev_state.scene_minor != SCENE_VS_ONLINE_VERSUS
            && state.scene_minor == SCENE_VS_ONLINE_VERSUS;
        let vs_screen_2 = prev_state.scene_minor == SCENE_VS_ONLINE_VERSUS && state.stage_id == 0;
        let entered_vs_online_opponent_screen = vs_screen_1 || vs_screen_2;

        if state.scene_major == SCENE_VS_ONLINE
            && prev_state.scene_minor != SCENE_VS_ONLINE_RANKED
            && state.scene_minor == SCENE_VS_ONLINE_RANKED
        {
            MeleeEvent::RankedStageStrikeEntered
        } else if !prev_state.in_menus && state.in_menus {
            MeleeEvent::MenuEntered
        } else if prev_state.scene_major != SCENE_TITLE_SCREEN && state.scene_major == SCENE_TITLE_SCREEN {
            MeleeEvent::TitleScreenEntered
        } else if entered_vs_online_opponent_screen {
            MeleeEvent::VsOnlineOpponent
        } else if prev_state.scene_major != SCENE_TROPHY_LOTTERY && state.scene_major == SCENE_TROPHY_LOTTERY {
            MeleeEvent::LotteryEntered
        } else if (!prev_state.in_game && state.in_game) || prev_state.stage_id != state.stage_id {
            MeleeEvent::GameStart(state.stage_id)
        } else if prev_state.in_game && state.in_game && state.match_info == 1 {
            MeleeEvent::GameEnd
        } else if prev_state.volume != state.volume {
            MeleeEvent::SetVolume(state.volume)
        } else if !prev_state.is_paused && state.is_paused {
            MeleeEvent::Pause
        } else if prev_state.is_paused && !state.is_paused {
            MeleeEvent::Unpause
        } else {
            MeleeEvent::NoOp
        }
    }

    /// Create a `DolphinGameState` by reading Dolphin's memory
    fn read_dolphin_game_state(m_p_ram: &usize, dolphin_volume_percent: f32) -> Result<DolphinGameState> {
        #[inline(always)]
        fn read<T: Copy>(offset: usize) -> Result<T> {
            Ok(unsafe { LocalMember::<T>::new_offset(vec![offset]).read()? })
        }
        // https://github.com/bkacjios/m-overlay/blob/d8c629d/source/modules/games/GALE01-2.lua#L8
        let melee_volume_percent = ((read::<i8>(m_p_ram + 0x45C384)? as f32 - 100.0) * -1.0) / 100.0;
        // https://github.com/bkacjios/m-overlay/blob/d8c629d/source/modules/games/GALE01-2.lua#L16
        let scene_major = read::<u8>(m_p_ram + 0x479D30)?;
        // https://github.com/bkacjios/m-overlay/blob/d8c629d/source/modules/games/GALE01-2.lua#L19
        let scene_minor = read::<u8>(m_p_ram + 0x479D33)?;
        // https://github.com/bkacjios/m-overlay/blob/d8c629d/source/modules/games/GALE01-2.lua#L357
        let stage_id = read::<u8>(m_p_ram + 0x49E753)?;
        // https://github.com/bkacjios/m-overlay/blob/d8c629d/source/modules/games/GALE01-2.lua#L248
        // 0 = in game, 1 = GAME! screen, 2 = Stage clear in 1p mode? (maybe also victory screen), 3 = menu
        let match_info = read::<u8>(m_p_ram + 0x46B6A0)?;
        // https://github.com/bkacjios/m-overlay/blob/d8c629d/source/modules/games/GALE01-2.lua#L353
        let is_paused = read::<u8>(m_p_ram + 0x4D640F)? == 1;

        Ok(DolphinGameState {
            in_game: utils::is_in_game(scene_major, scene_minor),
            in_menus: utils::is_in_menus(scene_major, scene_minor),
            scene_major,
            scene_minor,
            volume: dolphin_volume_percent * melee_volume_percent * VOLUME_REDUCTION_MULTIPLIER,
            stage_id,
            is_paused,
            match_info,
        })
    }
}

impl Drop for Jukebox {
    fn drop(&mut self) {
        tracing::info!(target: Log::Jukebox, "Dropping Slippi Jukebox");
        for sender in &self.channel_senders {
            if let Err(e) = sender.send(JukeboxEvent::Dropped) {
                tracing::error!(
                    target: Log::Jukebox,
                    "Failed to notify child thread that Jukebox is dropping: {e}"
                );
            }
        }
    }
}

// This wrapper allows us to implement `rodio::Source`
struct HpsAudioSource {
    pcm: PcmIterator,
    padding_length: u32,
}

impl Iterator for HpsAudioSource {
    type Item = i16;

    fn next(&mut self) -> Option<Self::Item> {
        // We need to pad the start of the music playback with a quarter second
        // of silence so when two tracks are loaded in quick succession, we
        // don't hear a quick "blip" from the first track. This happens in
        // practice because scene_minor tells us we're in-game before stage_id
        // has a chance to update from the previously played stage.
        //
        // Return 0s (silence) for the length of the padding
        if self.padding_length > 0 {
            self.padding_length -= 1;
            return Some(0);
        }
        // Then start iterating on the actual samples
        self.pcm.next()
    }
}

impl rodio::Source for HpsAudioSource {
    fn current_frame_len(&self) -> Option<usize> {
        None
    }
    fn channels(&self) -> u16 {
        self.pcm.channel_count as u16
    }
    fn sample_rate(&self) -> u32 {
        self.pcm.sample_rate
    }
    fn total_duration(&self) -> Option<std::time::Duration> {
        None
    }
}
