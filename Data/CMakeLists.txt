file(GLOB ALL_GAMEINIS ${CMAKE_CURRENT_SOURCE_DIR}/GameSettings/*)
file(
  GLOB ALL_GAMEINIS_BASENAMES
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/GameSettings/
  ${CMAKE_CURRENT_SOURCE_DIR}/GameSettings/*
)
add_custom_command(
  OUTPUT GameSettings.zip
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/GameSettings
  COMMAND
    ${CMAKE_COMMAND} -E tar cf ${CMAKE_CURRENT_BINARY_DIR}/GameSettings.zip
    --format=zip ${ALL_GAMEINIS_BASENAMES}
  DEPENDS ${ALL_GAMEINIS}
)
add_custom_target(game_settings_zip ALL DEPENDS GameSettings.zip)
