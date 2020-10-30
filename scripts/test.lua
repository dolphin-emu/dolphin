-- Test memory access
print(dolphin.mem_read(0x80000000, 6))

-- Test basic instruction hooks
main_hook = function ()
  print("main")
end
dolphin.hook_instruction(0x80008ef0, main_hook)
dolphin.hook_instruction(0x80008ef0, main_hook) -- should be no-op

-- Test instruction hook registry
dummy_instruction_hook = function ()
  print("SHOULD NOT SEE THIS")
end
dolphin.hook_instruction(0x80008ef0, dummy_instruction_hook)
dolphin.hook_instruction(0x80008ef0, dummy_instruction_hook) -- should be no-op
dolphin.unhook_instruction(0x80008ef0, dummy_instruction_hook) -- should remove the hook

-- Actually log something
layouts = {}
dolphin.hook_instruction(0x805e889c, function ()
  lyt_name_ptr = dolphin.get_gpr(4)
  lyt_name = dolphin.str_read(lyt_name_ptr, 100)
  if layouts[lyt_name] == nil then
    print(lyt_name)
    layouts[lyt_name] = true
  end
end)

-- Test frame hook registry
dummy_frame_hook = function ()
  print("SHOULD NOT SEE THIS")
end
dolphin.hook_frame(dummy_frame_hook)
dolphin.hook_frame(dummy_frame_hook)
dolphin.unhook_frame(dummy_frame_hook)

-- Print frame count every now and then
frame_count = 0
dolphin.hook_frame(function ()
  frame_count = frame_count + 1
  if frame_count % 120 == 0 then
    print("frame " .. frame_count)
  end
end)
