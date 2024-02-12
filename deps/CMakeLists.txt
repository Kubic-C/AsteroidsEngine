
add_subdirectory(bitsery)
add_subdirectory(GameNetworkingSockets)
set(SFML_BUILD_GRAPHICS true)
add_subdirectory(SFML)
add_subdirectory(flecs)
add_subdirectory(THST)
set(TGUI_BACKEND SFML_GRAPHICS)
add_subdirectory(TGUI)