
add_library(AsteroidsEngine STATIC 
	"includes.hpp" 
	"engine.hpp" "engine.cpp" 
	"logging.hpp" "state.hpp" 
	"entry.hpp" 
	"time.hpp"
	"network.hpp"
	"physics.hpp"
	"core.hpp")

target_include_directories(AsteroidsEngine PUBLIC "${CMAKE_CURRENT_SRC_DIR}")
target_link_libraries(AsteroidsEngine PUBLIC 	
	bitsery 
	GameNetworkingSockets::static
	sfml-system
	sfml-graphics
    sfml-audio
    flecs_static
	THST
	tgui)


