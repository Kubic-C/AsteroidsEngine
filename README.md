# Asteroids Engine

The Asteroids engine is a C++ 2D engine.

Features:
  * Simplistic Networking interface (with automatic syncing of entities and components, and is quite resilient to lag)
  * Networked components and entities
  * Networked 2D physics
  * GUI system (powered by TGUI)
  * Window and audio system (powered by SFML)
  * Logging and error handling system

The engine is named after where it originated from, the game Asteroids. 
_**Documentation is a work in progress,**_ but most of the code is self-explanatory.
  
## Libraries

There are many libraries that the asteroids engine uses:
  * bitsery: For serialization and deserialization of networking messages
  * flecs: Entity-Component-System handling
  * GameNetworkingSockets: Steam's networking library, used for of course networking
  * SFML: Audio, Window, Rendering handling
  * TGUI: GUI handling
  * THST: Spatial partitioning library
  * nlohman/json: For config reading and writing
  * boost: various utility functions

## Important for those building on Windows

It is **required** that you use VCPKG to build this engine, it is part of this engine's toolchain on Windows.
Below is a step by step guide on how to install vcpkg

Installing vcpkg:
```
$ git clone https://github.com/microsoft/vcpkg
$ .\vcpkg\bootstrap-vcpkg.bat
$ .\vcpkg\vcpkg integrate install
```

Installing the neccessary packages with vcpkg
```
.\vcpkg\vcpkg install GameNetworkingSockets --triplet=x64-windows
.\vcpkg\vcpkg install protobuf --triplet=x64-windows
```

This is a crucial step! Guide yourself to the "Edit Enviroment Variables for your account"
panel, next click "new." In the "variable name" field type "VCPKG_ROOT," and then under
the "variable value" field insert the file path of where you cloned vcpkg, including the
vcpkg directory itself. The variable value field should look something like this:
"C:/My/path/to/the/thing/vcpkg"

Finally, in between where you call cmake_minimum_required() and your project(), insert this:
```
if(WIN32)
    message("Using the VCPKG Toolchain for Windows");
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}")
endif()
```