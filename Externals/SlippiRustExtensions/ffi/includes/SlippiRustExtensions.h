#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

/// This enum is duplicated from `slippi_game_reporter::OnlinePlayMode` in order
/// to appease cbindgen, which cannot see the type from the other module for
/// inspection.
///
/// This enum will likely go away as things move towards Rust, since it's effectively
/// just C FFI glue code.
enum SlippiMatchmakingOnlinePlayMode {
  Ranked = 0,
  Unranked = 1,
  Direct = 2,
  Teams = 3,
};

extern "C" {

/// Creates and leaks a shadow EXI device.
///
/// The C++ (Dolphin) side of things should call this and pass the appropriate arguments. At
/// that point, everything on the Rust side is its own universe, and should be told to shut
/// down (at whatever point) via the corresponding `slprs_exi_device_destroy` function.
///
/// The returned pointer from this should *not* be used after calling `slprs_exi_device_destroy`.
uintptr_t slprs_exi_device_create(const char *iso_path,
                                  void (*osd_add_msg_fn)(const char*, uint32_t, uint32_t));

/// The C++ (Dolphin) side of things should call this to notify the Rust side that it
/// can safely shut down and clean up.
void slprs_exi_device_destroy(uintptr_t exi_device_instance_ptr);

/// This method should be called from the EXI device subclass shim that's registered on
/// the Dolphin side, corresponding to:
///
/// `virtual void DMAWrite(u32 _uAddr, u32 _uSize);`
void slprs_exi_device_dma_write(uintptr_t exi_device_instance_ptr,
                                const uint8_t *address,
                                const uint8_t *size);

/// This method should be called from the EXI device subclass shim that's registered on
/// the Dolphin side, corresponding to:
///
/// `virtual void DMARead(u32 _uAddr, u32 _uSize);`
void slprs_exi_device_dma_read(uintptr_t exi_device_instance_ptr,
                               const uint8_t *address,
                               const uint8_t *size);

/// Moves ownership of the `GameReport` at the specified address to the
/// `SlippiGameReporter` on the EXI Device the corresponding address. This
/// will then add it to the processing pipeline.
///
/// The reporter will manage the actual... reporting.
void slprs_exi_device_log_game_report(uintptr_t instance_ptr, uintptr_t game_report_instance_ptr);

/// Calls through to `SlippiGameReporter::start_new_session`.
void slprs_exi_device_start_new_reporter_session(uintptr_t instance_ptr);

/// Calls through to the `SlippiGameReporter` on the EXI device to report a
/// match completion event.
void slprs_exi_device_report_match_completion(uintptr_t instance_ptr,
                                              const char *uid,
                                              const char *play_key,
                                              const char *match_id,
                                              uint8_t end_mode);

/// Calls through to the `SlippiGameReporter` on the EXI device to report a
/// match abandon event.
void slprs_exi_device_report_match_abandonment(uintptr_t instance_ptr,
                                               const char *uid,
                                               const char *play_key,
                                               const char *match_id);

/// Calls through to `SlippiGameReporter::push_replay_data`.
void slprs_exi_device_reporter_push_replay_data(uintptr_t instance_ptr,
                                                const uint8_t *data,
                                                uint32_t length);

/// Configures the Jukebox process. This needs to be called after the EXI device is created
/// in order for certain pieces of Dolphin to be properly initalized; this may change down
/// the road though and is not set in stone.
void slprs_exi_device_configure_jukebox(uintptr_t exi_device_instance_ptr,
                                        bool is_enabled,
                                        const uint8_t *m_p_ram,
                                        int (*get_dolphin_volume_fn)());

/// Creates a new Player Report and leaks it, returning the pointer.
///
/// This should be passed on to a GameReport for processing.
uintptr_t slprs_player_report_create(const char *uid,
                                     uint8_t slot_type,
                                     double damage_done,
                                     uint8_t stocks_remaining,
                                     uint8_t character_id,
                                     uint8_t color_id,
                                     int64_t starting_stocks,
                                     int64_t starting_percent);

/// Creates a new GameReport and leaks it, returning the instance pointer
/// after doing so.
///
/// This is expected to ultimately be passed to the game reporter, which will handle
/// destruction and cleanup.
uintptr_t slprs_game_report_create(const char *uid,
                                   const char *play_key,
                                   SlippiMatchmakingOnlinePlayMode online_mode,
                                   const char *match_id,
                                   uint32_t duration_frames,
                                   uint32_t game_index,
                                   uint32_t tie_break_index,
                                   int8_t winner_index,
                                   uint8_t game_end_method,
                                   int8_t lras_initiator,
                                   int32_t stage_id);

/// Takes ownership of the `PlayerReport` at the specified pointer, adding it to the
/// `GameReport` at the corresponding pointer.
void slprs_game_report_add_player_report(uintptr_t instance_ptr,
                                         uintptr_t player_report_instance_ptr);

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
void slprs_logging_init(void (*logger_fn)(int, int, const char*));

/// Registers a log container, which mirrors a Dolphin `LogContainer` (`RustLogContainer`).
///
/// See `dolphin_logger::register_container` for more information.
void slprs_logging_register_container(const char *kind,
                                      int log_type,
                                      bool is_enabled,
                                      int default_log_level);

/// Updates the configuration for a registered logging container.
///
/// For more information, see `dolphin_logger::update_container`.
void slprs_logging_update_container(const char *kind, bool enabled, int level);

/// Updates the configuration for registered logging container on mainline
///
/// For more information, see `dolphin_logger::update_container`.
void slprs_mainline_logging_update_log_level(int level);

} // extern "C"
