-- ======= ----- ======= --
-- ======= TESTS ======= --
-- ======= ----- ======= --

-- Test register bounds
if dolphin.ppc.gpr[-1] ~= nil or dolphin.ppc.gpr[32] ~= nil then
  error("gpr out of bounds access")
end
if dolphin.ppc.ps_f64[-1] ~= nil or dolphin.ppc.ps_f64[32] ~= nil then
  error("ps out of bounds access")
end
if dolphin.ppc.ps_f64[0][-1] ~= nil or dolphin.ppc.ps_f64[0][2] ~= nil then
  error("ps elem out of bounds access")
end

-- Test frame hook registry
dummy_frame_hook = function ()
  error("Frame hook executed erroneously")
end
dolphin.hook_frame(dummy_frame_hook)
dolphin.hook_frame(dummy_frame_hook)
dolphin.unhook_frame(dummy_frame_hook)

-- Test instruction hook registry
dummy_instruction_hook = function ()
  error("Instruction hook executed erroneously")
end
dolphin.hook_instruction(0x800060a4, dummy_instruction_hook)
dolphin.hook_instruction(0x800060a4, dummy_instruction_hook) -- should be no-op
dolphin.unhook_instruction(0x800060a4, dummy_instruction_hook) -- should remove the hook

-- Test instruction hook on entrypoint
hook_invocations = 0
dolphin.hook_instruction(0x800060a4, function ()
  hook_invocations = hook_invocations + 1
end)
dolphin.hook_frame(function ()
  if hook_invocations ~= 1 then
    error("Odd number of entrypoint hook invocations: " .. hook_invocations)
  end
end)
