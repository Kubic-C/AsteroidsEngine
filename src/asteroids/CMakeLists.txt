
add_library(AsteroidsEngine STATIC 
	"includes.hpp" 
	"engine.hpp" "engine.cpp" 
	"logging.hpp" "state.hpp" 
	"time.hpp"
	"network.hpp"
	"physics.hpp"
	"core.hpp" 
	"entry.hpp" "entry.cpp"
	"asteroids.hpp")
	 
set_target_properties(AsteroidsEngine PROPERTIES COMPILE_FLAGS "-w")

target_include_directories(AsteroidsEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(AsteroidsEngine PUBLIC 	
	bitsery 
	GameNetworkingSockets::static
	sfml-system
	sfml-graphics
    sfml-audio
    flecs_static
	THST
	tgui)


