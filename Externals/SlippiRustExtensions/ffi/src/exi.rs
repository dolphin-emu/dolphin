use std::ffi::{c_char, c_int};

use dolphin_integrations::Log;
use slippi_exi_device::SlippiEXIDevice;
use slippi_game_reporter::GameReport;

use crate::c_str_to_string;

/// Creates and leaks a shadow EXI device.
///
/// The C++ (Dolphin) side of things should call this and pass the appropriate arguments. At
/// that point, everything on the Rust side is its own universe, and should be told to shut
/// down (at whatever point) via the corresponding `slprs_exi_device_destroy` function.
///
/// The returned pointer from this should *not* be used after calling `slprs_exi_device_destroy`.
#[no_mangle]
pub extern "C" fn slprs_exi_device_create(
    iso_path: *const c_char,
    osd_add_msg_fn: unsafe extern "C" fn(*const c_char, u32, u32),
) -> usize {
    let fn_name = "slprs_exi_device_create";

    let iso_path = c_str_to_string(iso_path, fn_name, "iso_path");

    dolphin_integrations::ffi::osd::set_global_hook(osd_add_msg_fn);

    let exi_device = Box::new(SlippiEXIDevice::new(iso_path));
    let exi_device_instance_ptr = Box::into_raw(exi_device) as usize;

    tracing::warn!(target: Log::EXI, ptr = exi_device_instance_ptr, "Creating Device");

    exi_device_instance_ptr
}

/// The C++ (Dolphin) side of things should call this to notify the Rust side that it
/// can safely shut down and clean up.
#[no_mangle]
pub extern "C" fn slprs_exi_device_destroy(exi_device_instance_ptr: usize) {
    tracing::warn!(target: Log::EXI, ptr = exi_device_instance_ptr, "Destroying Device");

    // Coerce the instance from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `exi_device_instance_ptr` is only owned
    // by the C++ EXI device, and is created/destroyed with the corresponding lifetimes.
    unsafe {
        // Coerce ownership back, then let standard Drop semantics apply
        let _device = Box::from_raw(exi_device_instance_ptr as *mut SlippiEXIDevice);
    }
}

/// This method should be called from the EXI device subclass shim that's registered on
/// the Dolphin side, corresponding to:
///
/// `virtual void DMAWrite(u32 _uAddr, u32 _uSize);`
#[no_mangle]
pub extern "C" fn slprs_exi_device_dma_write(exi_device_instance_ptr: usize, address: *const u8, size: *const u8) {
    // Coerce the instance back from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `exi_device_instance_ptr` pointer is only owned
    // by the C++ EXI device, and is created/destroyed with the corresponding lifetimes.
    let mut device = unsafe { Box::from_raw(exi_device_instance_ptr as *mut SlippiEXIDevice) };

    device.dma_write(address as usize, size as usize);

    // Fall back into a raw pointer so Rust doesn't obliterate the object
    let _leak = Box::into_raw(device);
}

/// This method should be called from the EXI device subclass shim that's registered on
/// the Dolphin side, corresponding to:
///
/// `virtual void DMARead(u32 _uAddr, u32 _uSize);`
#[no_mangle]
pub extern "C" fn slprs_exi_device_dma_read(exi_device_instance_ptr: usize, address: *const u8, size: *const u8) {
    // Coerce the instance from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `exi_device_instance_ptr` pointer is only owned
    // by the C++ EXI device, and is created/destroyed with the corresponding lifetimes.
    let mut device = unsafe { Box::from_raw(exi_device_instance_ptr as *mut SlippiEXIDevice) };

    device.dma_read(address as usize, size as usize);

    // Fall back into a raw pointer so Rust doesn't obliterate the object.
    let _leak = Box::into_raw(device);
}

/// Moves ownership of the `GameReport` at the specified address to the
/// `SlippiGameReporter` on the EXI Device the corresponding address. This
/// will then add it to the processing pipeline.
///
/// The reporter will manage the actual... reporting.
#[no_mangle]
pub extern "C" fn slprs_exi_device_log_game_report(instance_ptr: usize, game_report_instance_ptr: usize) {
    // Coerce the instances from the pointers. This is theoretically safe since we control
    // the C++ side and can guarantee that the pointers are only owned
    // by us, and are created/destroyed with the corresponding lifetimes.
    let (mut device, game_report) = unsafe {
        (
            Box::from_raw(instance_ptr as *mut SlippiEXIDevice),
            Box::from_raw(game_report_instance_ptr as *mut GameReport),
        )
    };

    device.game_reporter.log_report(*game_report);

    // Fall back into a raw pointer so Rust doesn't obliterate the object.
    let _leak = Box::into_raw(device);
}

/// Calls through to `SlippiGameReporter::start_new_session`.
#[no_mangle]
pub extern "C" fn slprs_exi_device_start_new_reporter_session(instance_ptr: usize) {
    // Coerce the instances from the pointers. This is theoretically safe since we control
    // the C++ side and can guarantee that the pointers are only owned
    // by us, and are created/destroyed with the corresponding lifetimes.
    let mut device = unsafe { Box::from_raw(instance_ptr as *mut SlippiEXIDevice) };

    device.game_reporter.start_new_session();

    // Fall back into a raw pointer so Rust doesn't obliterate the object.
    let _leak = Box::into_raw(device);
}

/// Calls through to the `SlippiGameReporter` on the EXI device to report a
/// match completion event.
#[no_mangle]
pub extern "C" fn slprs_exi_device_report_match_completion(
    instance_ptr: usize,
    uid: *const c_char,
    play_key: *const c_char,
    match_id: *const c_char,
    end_mode: u8,
) {
    // Coerce the instances from the pointers. This is theoretically safe since we control
    // the C++ side and can guarantee that the pointers are only owned
    // by us, and are created/destroyed with the corresponding lifetimes.
    let device = unsafe { Box::from_raw(instance_ptr as *mut SlippiEXIDevice) };

    let fn_name = "slprs_exi_device_report_match_completion";
    let uid = c_str_to_string(uid, fn_name, "uid");
    let play_key = c_str_to_string(play_key, fn_name, "play_key");
    let match_id = c_str_to_string(match_id, fn_name, "match_id");

    device.game_reporter.report_completion(uid, play_key, match_id, end_mode);

    // Fall back into a raw pointer so Rust doesn't obliterate the object.
    let _leak = Box::into_raw(device);
}

/// Calls through to the `SlippiGameReporter` on the EXI device to report a
/// match abandon event.
#[no_mangle]
pub extern "C" fn slprs_exi_device_report_match_abandonment(
    instance_ptr: usize,
    uid: *const c_char,
    play_key: *const c_char,
    match_id: *const c_char,
) {
    // Coerce the instances from the pointers. This is theoretically safe since we control
    // the C++ side and can guarantee that the pointers are only owned
    // by us, and are created/destroyed with the corresponding lifetimes.
    let device = unsafe { Box::from_raw(instance_ptr as *mut SlippiEXIDevice) };

    let fn_name = "slprs_exi_device_report_match_abandonment";
    let uid = c_str_to_string(uid, fn_name, "uid");
    let play_key = c_str_to_string(play_key, fn_name, "play_key");
    let match_id = c_str_to_string(match_id, fn_name, "match_id");

    device.game_reporter.report_abandonment(uid, play_key, match_id);

    // Fall back into a raw pointer so Rust doesn't obliterate the object.
    let _leak = Box::into_raw(device);
}

/// Calls through to `SlippiGameReporter::push_replay_data`.
#[no_mangle]
pub extern "C" fn slprs_exi_device_reporter_push_replay_data(instance_ptr: usize, data: *const u8, length: u32) {
    // Convert our pointer to a Rust slice so that the game reporter
    // doesn't need to deal with anything C-ish.
    let slice = unsafe { std::slice::from_raw_parts(data, length as usize) };

    // Coerce the instances from the pointers. This is theoretically safe since we control
    // the C++ side and can guarantee that the pointers are only owned
    // by us, and are created/destroyed with the corresponding lifetimes.
    let mut device = unsafe { Box::from_raw(instance_ptr as *mut SlippiEXIDevice) };

    device.game_reporter.push_replay_data(slice);

    // Fall back into a raw pointer so Rust doesn't obliterate the object.
    let _leak = Box::into_raw(device);
}

/// Configures the Jukebox process. This needs to be called after the EXI device is created
/// in order for certain pieces of Dolphin to be properly initalized; this may change down
/// the road though and is not set in stone.
#[no_mangle]
pub extern "C" fn slprs_exi_device_configure_jukebox(
    exi_device_instance_ptr: usize,
    is_enabled: bool,
    m_p_ram: *const u8,
    get_dolphin_volume_fn: unsafe extern "C" fn() -> c_int,
) {
    // Coerce the instance from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `exi_device_instance_ptr` is only owned
    // by the C++ EXI device, and is created/destroyed with the corresponding lifetimes.
    let mut device = unsafe { Box::from_raw(exi_device_instance_ptr as *mut SlippiEXIDevice) };

    device.configure_jukebox(is_enabled, m_p_ram, get_dolphin_volume_fn);

    // Fall back into a raw pointer so Rust doesn't obliterate the object.
    let _leak = Box::into_raw(device);
}
