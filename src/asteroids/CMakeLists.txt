
add_library(AsteroidsEngine STATIC 
	"includes.hpp" 
	"engine.hpp" "engine.cpp" 
	"logging.hpp" "state.hpp" 
	"time.hpp"
	"network.hpp"
	"physics.hpp"
	"core.hpp" 
	"config.hpp"
	"entry.hpp" "entry.cpp"
	"asteroids.hpp")

if(MSVC)
	target_compile_options(AsteroidsEngine PRIVATE "/permissive-")
endif()

set(Boost_USE_STATIC_LIBS   ON)
set(Boost_ROOT "$ENV{BOOST_ROOT}")
find_package(Boost REQUIRED)
target_include_directories(AsteroidsEngine PUBLIC "${Boost_INCLUDE_DIRS}")
target_link_directories(AsteroidsEngine PUBLIC "${Boost_LIBRARY_DIRS}")

target_include_directories(AsteroidsEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(AsteroidsEngine PUBLIC 	
	bitsery 
	GameNetworkingSockets::static
	sfml-system
	sfml-graphics
    sfml-audio
    flecs
	THST
	tgui
    nlohmann_json::nlohmann_json)


