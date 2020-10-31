-- General purpose registers
for i, gpr in ipairs(dolphin.ppc.gpr) do
  print(string.format("gpr[%d] = %x", i, gpr))
end
print(string.format("pc = %x", dolphin.ppc.pc))

-- Paired single registers
for i, ps_reg in ipairs(dolphin.ppc.ps_f64) do
  print(string.format("ps[%d][0] = %f", i, ps_reg[0]))
  print(string.format("ps[%d][1] = %f", i, ps_reg[1]))
end

-- Basic memory access
print(dolphin.mem_read(0x80000000, 6))
if dolphin.mem_u32[0x80000000] ~= 0x524d4350 then
  error("wrong game ID")
end

-- Log something
layouts = {}
dolphin.hook_instruction(0x805e889c, function ()
  lyt_name_ptr = dolphin.ppc.gpr[4]
  lyt_name = dolphin.str_read(lyt_name_ptr, 100)
  if layouts[lyt_name] == nil then
    print(lyt_name)
    layouts[lyt_name] = true
  end
end)

-- Print frame count every now and then
frame_count = 0
dolphin.hook_frame(function ()
  frame_count = frame_count + 1
  if frame_count % 120 == 0 then
    print("frame " .. frame_count)
  end
end)

-- ======= ----- ======= --
-- ======= TESTS ======= --
-- ======= ----- ======= --

-- Test basic instruction hooks
main_hook = function ()
  print("main")
end
dolphin.hook_instruction(0x80008ef0, main_hook)
dolphin.hook_instruction(0x80008ef0, main_hook) -- should be no-op

-- Test frame hook registry
dummy_frame_hook = function ()
  print("SHOULD NOT SEE THIS")
end
dolphin.hook_frame(dummy_frame_hook)
dolphin.hook_frame(dummy_frame_hook)
dolphin.unhook_frame(dummy_frame_hook)

-- Test instruction hook registry
dummy_instruction_hook = function ()
  print("SHOULD NOT SEE THIS")
end
dolphin.hook_instruction(0x80008ef0, dummy_instruction_hook)
dolphin.hook_instruction(0x80008ef0, dummy_instruction_hook) -- should be no-op
dolphin.unhook_instruction(0x80008ef0, dummy_instruction_hook) -- should remove the hook
