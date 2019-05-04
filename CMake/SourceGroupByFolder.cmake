# This function should be passed a name of an existing target. It will automatically generate
# file groups following the directory hierarchy, so that the layout of the files in IDEs matches the
# one in the filesystem.
function(source_group_by_folder target_name)
  # Place any files that aren't in the source list in a separate group so that they don't get in
  # the way.
  source_group("Other Files" REGULAR_EXPRESSION ".")

  get_target_property(target_sources "${target_name}" SOURCES)

  foreach(file_name IN LISTS target_sources)
      get_filename_component(dir_name "${file_name}" PATH)
      # Group names use '\' as a separator even though the entire rest of CMake uses '/'...
      string(REPLACE "/" "\\" group_name "${dir_name}")
      source_group("${group_name}" FILES "${file_name}")
  endforeach()
endfunction()
