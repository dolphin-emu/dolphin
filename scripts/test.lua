print(dolphin.memcpy(0x80000000, 6))

-- Quick check if it works
dolphin.add_hook(0x80008ef0, function ()
  print("YOOO")
end)

-- Sanity check the multimap stuff
test_func = function ()
  print("SHOULD NOT SEE THIS")
end
dolphin.add_hook(0x80000000, test_func)
dolphin.add_hook(0x80000000, test_func)
dolphin.remove_hook(0x80000000, test_func)

layouts = {}

-- Proof-of-concept
dolphin.add_hook(0x805e889c, function ()
  lyt_name_ptr = dolphin.get_gpr(4)
  lyt_name = dolphin.strncpy(lyt_name_ptr, 100)
  if layouts[lyt_name] == nil then
    print(lyt_name)
    layouts[lyt_name] = true
  end
end)
