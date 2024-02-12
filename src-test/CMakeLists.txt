
add_executable(test_field "main.cpp")

target_link_libraries(test_field PUBLIC scrap)

add_custom_command(TARGET test_field POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:test_field> $<TARGET_FILE_DIR:test_field>
  COMMAND_EXPAND_LISTS
  )