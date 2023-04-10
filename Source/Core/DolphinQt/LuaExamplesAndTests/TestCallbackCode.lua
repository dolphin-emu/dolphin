require ("emu")

on_frame_start_ref = -1
gc_poll_ref = -1
wii_poll_ref = -1

function print_callback_statuses()
	print("\tIn frameStart callback was " .. tostring(OnFrameStart:isInFrameStartCallback()))
	print("\tIn OnGCControllerPolled callback was " .. tostring(OnGCControllerPolled:isInGCControllerPolledCallback()))
	print("\tIn OnInstructionHit callback was " .. tostring(OnInstructionHit:isInInstructionHitCallback()))
	print("\tIn OnMemoryAddressReadFrom callback was " .. tostring(OnMemoryAddressReadFrom:isInMemoryAddressReadFromCallback()))
	print("\tIn OnMemoryAddressWrittenTo callback was " .. tostring(OnMemoryAddressWrittenTo:isInMemoryAddressWrittenToCallback()))
	print("\tIn OnWiiInputPolled callback was " .. tostring(OnWiiInputPolled:isInWiiInputPolledCallback()))
end

function on_frame_start()
	print("Inside of an OnFrameStart callback function...\n")
	print_callback_statuses()
	OnFrameStart:unregister(on_frame_start_ref)
end

function on_gc_cont_polled()
	print("Inside of an OnGCControllerPolled callback function...\n")
	print_callback_statuses()
	OnGCControllerPolled:unregister(gc_poll_ref)
end

function on_wii_polled()
	print("Inside of an OnWiiInputPolled callback function...\n")
	print_callback_statuses()
	OnWiiInputPolled:unregister(wii_poll_ref)
end

on_frame_start_ref = OnFrameStart:register(on_frame_start)
gc_poll_ref = OnGCControllerPolled:register(on_gc_cont_polled)
wii_poll_ref = OnWiiInputPolled:register(on_wii_polled)

print("At script startup...\n")
print_callback_statuses()
emu:frameAdvance()
emu:frameAdvance()
print("At global code in start of frame...\n")
print_callback_statuses()