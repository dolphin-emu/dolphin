add_executable(dsptool Src/DSPTool.cpp)
target_link_libraries(dsptool core)
install(TARGETS dsptool RUNTIME DESTINATION ${bindir})
