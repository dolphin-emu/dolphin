print(dolphin.mem_read(0x80000000, 6))

-- Quick check if it works
main_hook = function ()
  print("main")
end
dolphin.hook_instruction(0x80008ef0, main_hook)
dolphin.hook_instruction(0x80008ef0, main_hook) -- should be no-op

-- Sanity check the multimap stuff
test_func = function ()
  print("SHOULD NOT SEE THIS")
end
dolphin.hook_instruction(0x80008ef0, test_func)
dolphin.hook_instruction(0x80008ef0, test_func) -- should be no-op
dolphin.unhook_instruction(0x80008ef0, test_func) -- should remove the hook

layouts = {}

-- Proof-of-concept
dolphin.hook_instruction(0x805e889c, function ()
  lyt_name_ptr = dolphin.get_gpr(4)
  lyt_name = dolphin.str_read(lyt_name_ptr, 100)
  if layouts[lyt_name] == nil then
    print(lyt_name)
    layouts[lyt_name] = true
  end
end)
