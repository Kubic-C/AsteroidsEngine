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

The engine cannot currently be built with, or may be hard to build with MinGW compilers.
The reason is GameNetworkingSockets, there is some trouble compiling with the libraries
that GameNetworkingSockets needs.
  
## Libraries

There are many libraries that the asteroids engine uses:
  * bitsery: For serialization and deserialization of networking messages
  * flecs: Entity-Component-System handling
  * GameNetworkingSockets: Steam's networking library, used for of course networking
  * SFML: Audio, Window, Rendering handling
  * TGUI: GUI handling
  * THST: Spatial partitioning library
