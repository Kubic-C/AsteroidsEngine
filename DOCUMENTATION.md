# Documentation

The Asteroids Engine does different things based on two things:
  - The Active State
  - The Active Network Interface

These two things will define the behavior of your application

## Some things to know

The Asteroids Engine takes control of the entry point of your 
application, to give an example behind that look below:

```cpp
extern int EntryPoint(int argc, char* argv[]);

int main(int argc, char* argv[]) {
  // some custom init or something, yada yada

  EntryPoint(argc, argv);
  
  // and free resources
  return 0;
}

#define main EntryPoint

```

## State handling 

States can be defined within the Asteroids Engine as a set of
systems and functions. There are two parts to State in the Asteroids Engine,
the attached ECS Module, and then the actual State Type.

```cpp
/* The attached ECS Module */
struct MainMenuStateModule {
  MainMenuStateModule(flecs::world& world) {
    // define some systems and components using flecs.
  }
}

/* "The actual state type" */
class MainMenuState: public ae::State {
public:
  void onEntry() override {}

  void onLeave() override {}

  void onTick(float deltaTime) override {}

  void onUpdate() {}
private:
  // some cool data
}

/* "Another actual state type" */
class MainMenuStateNoModule: public ae::State {
public:
  void onEntry() override {}

  void onLeave() override {}

  void onTick(float deltaTime) override {}

  void onUpdate() {}
private:
  // some cool data
}


int main(int argc, char* argv[]) {

  // registering a state with an attached ECS module
  u64 stateId = ae::registerState<MainMenuState, MainMenuStateModule>();

  // registering a state with NO ECS module
  ae::registerState<MainMenuStateNoModule>();


  ae::transitionState<MainMenuState>();
  // or
  ae::transitionState(stateId);

  return 0;
} 
```
Transitioning to a state will make that state active.
This means:
  - Its attached ECS module will be enabled/active
  - SomeNewState::onEntry() will be called
    
For the previous state:
  -  Its attached ECS module will be disabled/inactive
  -  SomeOldState::onLeave() will  be called

### Why states

State handling and such is a very common pattern in all games. While
it may initially be confusing to think of state handling in an ECS,
the concept of modules clears it up quite a lot.

Modules in FLECS will have components and systems defined in their 
constructor; when this module is imported its components and systems 
are attached to that module and when the module's entity is 
disabled/enabled so are the components/systems. This allows for sets of
components/systems that fall under the same category of behavior to be defined together.
When a module is enabled/disabled so are the systems/components,
this can be thought of as turning ON or OFF the behavior.

## Networking

> **Not entirely complete**

One of the ways you interact with the network in Asteroids Engine is by using the inherited
classes of ```ae::NetworkInterface```.

There are two primary interfaces that you may inherit from.
```cpp
namespace ae {
  class ServerInterface: public ae::NetworkInterface {}
  class ClientInterface: public ae::NetworkInterface {}
}
```

***NOTE***: In the future, I plan to add a document that explains
how the Asteroids Engine handles networking, so if you have 
questions please wait :).

The server interface allows applications (that also use the ***Asteroids Engine***)
to connect to your application. To describe what the server interface does behind the scenes
in the simplest way possible, the Server will send copies of the ```flecs::world``` to the Clients.

There are functions that you may override in your XYZInterface class that are essentially a callback when
a message is received, a client joins, a client leaves, etc.

To send messages use the NetworkManager object that be accessed through ae::getNetworkManager().
There is one important rules when sending messages:
  - must **ALWAYS** have a 1-byte header serialized at the beginning of the message.
    Failing to do so will result in undefined behavior when a message is received client side.
```cpp
// ...

// In class that inherits ServerInterface
void onMessageRecieved(HSteamConnection conn, ae::MessageHeader header, Deserializer& des) {
  switch((CustomExtendedHeader)header) {
    case CustomExtendedHeader::someCoolRequest: {
      u16 someCool = 1337; // our magic number that the client wants
      ae::MessageBuffer buffer;
      buffer.startSerialize();
      buffer.serialize(CustomExtendedHeader::someCoolRequest);
      buffer.serialize(someCool);
      buffer.endSerialize();

      // send the message reliably and only to the connection/client conn.  
      ae::getNetworkManager().sendMessage(conn, std::move(buffer), false, true);
    } break;
  }

}

// ...
```

DOCUMENTATION todo:
  - Introduce the idea of component network priority
  - State Network Modules
  - Caveats.
