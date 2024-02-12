#include "engine.hpp"

AE_NAMESPACE_BEGIN

struct StateInfo {
	std::shared_ptr<State> state = nullptr;
	std::unordered_map<std::type_index, flecs::entity> networkModules;
};

/* for internal use only */
struct Engine {
	// ORDERED BY INITIALIZATION, DO NOT CHANGE THE ORDER- thank you

	flecs::world entityWorld;
	ISteamNetworkingSockets* sockets;
	ISteamNetworkingUtils* util;
	std::shared_ptr<NetworkManager> networkManager;
	std::shared_ptr<EntityWorldNetworkManager> entityWorldNetwork;
	Ticker<void(float)> ticker;
	std::function<void()> updateCallback;
	std::shared_ptr<sf::RenderWindow> window;
	std::shared_ptr<tgui::Gui> gui;
	std::shared_ptr<PhysicsWorld> physicsWorld;
	std::shared_ptr<PhysicsWorldNetworkManager> physicsWorldNetwork;
	std::unordered_map<u64, StateInfo> states;
	u64 activeState;
};

Engine* engine = nullptr; 

namespace impl {
	bool validState(u64 id) {
		return engine->states.find(id) != engine->states.end();
	}

	u64 _registerState(std::shared_ptr<State> ptr, std::type_index typeId) {
		if(typeId.hash_code() > UINT64_MAX)
			log(ERROR_SEVERITY_FATAL, "Hashcode of state surpassed max u64\n");

		if(validState((u64)typeId.hash_code()))
			log(ERROR_SEVERITY_FATAL, "State already registered: %s: %llu\n", typeId.name(), (u64)typeId.hash_code());

		engine->states[(u64)typeId.hash_code()].state = ptr;

		return (u64)typeId.hash_code();
	}

	void _registerNetworkStateModule(flecs::entity module, std::type_index networkInterfaceId, u64 stateId) {
		if(!validState(stateId))
			log(ERROR_SEVERITY_FATAL, "Cannot register Network State Module if the state is not already registered");

		engine->states[stateId].networkModules[networkInterfaceId] = module;
	}

	bool shouldExit() {
		return engine->entityWorld.should_quit();
	}

	ISteamNetworkingUtils* getUtils() {
		return engine->util;
	}
	
	ISteamNetworkingSockets* getSockets() {
		return engine->sockets;
	}

	void update() {
		sf::Event event;
		while(engine->window->pollEvent(event)) {
			switch(event.type) {
			case event.Closed:
				engine->entityWorld.quit();
				break;
			default:
				break;
			}

			engine->gui->handleEvent(event);
		}

		engine->window->clear();
		if(engine->updateCallback)
			engine->updateCallback();
		engine->states[engine->activeState].state->onUpdate();
		engine->gui->draw();
		engine->window->display();

		engine->networkManager->update();
	}

	void tick(float deltaTime) {
		engine->states[engine->activeState].state->onTick(deltaTime);
		engine->entityWorld.progress(deltaTime);	
	}
}

void init() {
	if(engine)
		log(ERROR_SEVERITY_FATAL, "Engine already initialized\n");

	engine = new Engine();
	
	// Networking
	SteamNetworkingErrMsg steamNetworkingErrMsg;
	if(!GameNetworkingSockets_Init(nullptr, steamNetworkingErrMsg))
		log(ERROR_SEVERITY_FATAL, "Unable to initialize steam networking library, %s\n", (const char*)&steamNetworkingErrMsg[0]);
	engine->sockets = SteamNetworkingSockets();
	engine->util = SteamNetworkingUtils();
	engine->networkManager = std::make_shared<NetworkManager>();
	engine->entityWorldNetwork = std::make_shared<EntityWorldNetworkManager>();
	CoreModule::registerCore();

	// Time
	engine->ticker.setRate(60.0f);
	engine->ticker.setFunction(impl::tick);

	// Window
	setWindow(std::make_shared<sf::RenderWindow>(sf::VideoMode(sf::Vector2u(800, 600)), "Default"));
	
	// Physics
	engine->physicsWorld = std::make_shared<PhysicsWorld>();
	engine->physicsWorldNetwork = std::make_shared<PhysicsWorldNetworkManager>();

	// Core
	engine->entityWorld.import<CoreModule>();

	// State
	u64 unknownStateId = registerState<UnknownState, UnknownModule>();
	engine->activeState = unknownStateId;
}

NetworkManager& getNetworkManager() {
	return *engine->networkManager;
}

flecs::world& getEntityWorld() {
	return engine->entityWorld;
}

EntityWorldNetworkManager& getEntityWorldNetworkManager() {
	return *engine->entityWorldNetwork;
}

PhysicsWorld& getPhysicsWorld() {
	return *engine->physicsWorld;
}

PhysicsWorldNetworkManager& getPhysicsWorldNetworkManager() {
	return *engine->physicsWorldNetwork;
}

Gui& getGui() {
	return *engine->gui;
}

State& getCurrentState() {
	return *engine->states[engine->activeState].state;
}

u64 getCurrentStateId() {
	return engine->activeState;
}

void transitionState(u64 stateId) {
	if(!impl::validState(stateId)) {
		log(ERROR_SEVERITY_WARNING, "Attempted to transition to a invalid state: %u\n", stateId);
		return;
	}

	if(stateId == engine->activeState)
		return;
	
	StateInfo& prevState = engine->states[engine->activeState];
	StateInfo& nextState = engine->states[stateId];

	prevState.state->getModule().disable();
	prevState.state->onLeave();
	engine->activeState = stateId;
	nextState.state->onEntry();
	nextState.state->getModule().enable();
	
	// enable NetworkStateModules
	NetworkManager& networkManager = *engine->networkManager;
	if(networkManager.hasNetworkInterface()) {
		NetworkInterface& interface = networkManager.getNetworkInterface();
		std::type_index id = std::type_index(typeid(interface));

		if(prevState.networkModules.find(id) != prevState.networkModules.end()) {
			prevState.networkModules[id].disable();
		}
		if (nextState.networkModules.find(id) != nextState.networkModules.end()) {
			nextState.networkModules[id].enable();
		}
	}
}

void setWindow(std::shared_ptr<sf::RenderWindow> window) {
	engine->window = window;
	engine->gui = std::make_shared<tgui::Gui>();
	engine->gui->setWindow(*engine->window);
}

sf::RenderWindow& getWindow() {
	return *engine->window;
}

void setFps(u32 fps) {
	engine->window->setFramerateLimit(fps);
}

void setTps(float tps) {
	engine->ticker.setRate(tps);
}

void setUpdateCallback(std::function<void()> callback) {
	engine->updateCallback = callback;
}

void mainLoop() {
	while(!impl::shouldExit()) {
		engine->ticker.update();
		impl::update();
	}
}

void free() {
	if(!engine)
		log(ERROR_SEVERITY_FATAL, "Engine has not been initialized\n");

	delete engine;

	GameNetworkingSockets_Kill();
}

AE_NAMESPACE_END