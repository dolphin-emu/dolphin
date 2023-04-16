require ("emu")
require ("registers")

dolphin:importModule("InstructionStepAPI", "1.0")

on_frame_start_ref = -1
gc_poll_ref = -1
wii_poll_ref = -1
mem_addr_written_ref = -1
mem_addr_read_ref = -1
instr_hit_ref = -1

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

function on_mem_write()
	print("Inside of an OnMemoryAddressWrittenTo callback function, with a memory address of " .. tostring(OnMemoryAddressWrittenTo:getMemoryAddressWrittenToForCurrentCallback()) .. ", and a value being written of " .. tostring(OnMemoryAddressWrittenTo:getValueWrittenToMemoryAddressForCurrentCallback()))
	print_callback_statuses()
	pc = registers:getRegister("PC", "u32")
	print("Current value of PC was: " .. tostring(pc))
	print("Current instruction was: " .. InstructionStepAPI:getInstructionFromAddress(pc))

	InstructionStepAPI:singleStep()
	pc = registers:getRegister("PC", "u32")
	print("After stepping, value of PC was: " .. tostring(pc))
	print("Current instruction was: " .. InstructionStepAPI:getInstructionFromAddress(pc))
	OnMemoryAddressWrittenTo:unregister(3422572554, mem_addr_written_ref)
end

function on_mem_read()
	print("Inside of an OnMemoryAddressReadFrom callback function, with a memory address of " .. tostring(OnMemoryAddressReadFrom:getMemoryAddressReadFromForCurrentCallback()))
	print_callback_statuses()
	pc = registers:getRegister("PC", "u32")
	print("Current value of PC was: " .. tostring(pc))
	print("Current instruction was: " .. InstructionStepAPI:getInstructionFromAddress(pc))

	InstructionStepAPI:singleStep()
	pc = registers:getRegister("PC", "u32")
	print("After stepping, value of PC was: " .. tostring(pc))
	print("Current instruction was: " .. InstructionStepAPI:getInstructionFromAddress(pc))
	OnMemoryAddressReadFrom:unregister(3422564352, mem_addr_read_ref)
end

function on_instr_hit()
	print("Inside of an OnInstructionHit callback function, with a PC address of " .. tostring(OnInstructionHit:getAddressOfInstructionForCurrentCallback()))
	print_callback_statuses()
	pc = registers:getRegister("PC", "u32")
	print("Current value of PC was: " .. tostring(pc))
	print("Current instruction was: " .. InstructionStepAPI:getInstructionFromAddress(pc))

	InstructionStepAPI:singleStep()
	pc = registers:getRegister("PC", "u32")
	print("After stepping, value of PC was: " .. tostring(pc))
	print("Current instruction was: " .. InstructionStepAPI:getInstructionFromAddress(pc))
	OnInstructionHit:unregister(2149867952, instr_hit_ref)
end
	

on_frame_start_ref = OnFrameStart:register(on_frame_start)
gc_poll_ref = OnGCControllerPolled:register(on_gc_cont_polled)
wii_poll_ref = OnWiiInputPolled:register(on_wii_polled)
mem_addr_written_ref = OnMemoryAddressWrittenTo:register(3422572554, on_mem_write)
mem_addr_read_ref = OnMemoryAddressReadFrom:register(3422564352, on_mem_read)
instr_hit_ref = OnInstructionHit:register(2149867952, on_instr_hit)

GC_Addr = 2149867952
Wii_Addr = 2147508776
print("At script startup...\n")
print_callback_statuses()
emu:frameAdvance()
emu:frameAdvance()
print("At global code in start of frame...\n")
print_callback_statuses()