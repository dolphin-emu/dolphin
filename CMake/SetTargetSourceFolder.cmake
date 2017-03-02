function(set_target_source_folder target)
  get_target_property(sources "${target}" SOURCES)
  list(SORT sources)
  set_target_properties("${target}" PROPERTIES SOURCES "${sources}")

  foreach(src IN LISTS sources)
    # We can't easily guess a folder for files that are referenced with
    # an absolute path, so we skip over them
    get_filename_component(abspath ${src} ABSOLUTE)
    if(src STREQUAL "${abspath}")
      continue()
    endif()

    get_filename_component(path ${src} DIRECTORY)

    string(REPLACE "/" "\\" path "${path}")
    source_group("${path}" FILES ${src})
  endforeach()
endfunction()
