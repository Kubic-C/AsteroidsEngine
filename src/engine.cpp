#include "engine.hpp"

/* for internal use only */
struct Engine {
	std::unordered_map<u64, std::shared_ptr<State>> states;
	std::shared_ptr<State> state;

	flecs::world entityWorld;
	std::shared_ptr<tgui::Gui> gui;
	std::shared_ptr<sf::RenderWindow> window;

	ISteamNetworkingSockets* sockets;
	ISteamNetworkingUtils* util;
	std::shared_ptr<NetworkManager> networkManager;
	std::shared_ptr<EntityWorldNetworkManager> entityWorldNetwork;

	std::shared_ptr<PhysicsWorld> physicsWorld;
	std::shared_ptr<PhysicsWorldNetworkManager> physicsWorldNetwork;

	Ticker<void(float)> ticker;

	std::function<void()> updateCallback;
};

Engine* engine = nullptr; 

namespace impl {
	bool validState(u64 id) {
		return engine->states.find(id) != engine->states.end();
	}

	u64 _registerState(std::shared_ptr<State> ptr, std::type_index typeId) {
		if(typeId.hash_code() > UINT64_MAX)
			engineLog(ERROR_SEVERITY_FATAL, "Hashcode of state surpassed max u64\n");

		if(validState((u64)typeId.hash_code()))
			engineLog(ERROR_SEVERITY_FATAL, "State already registered: %s: %llu\n", typeId.name(), (u64)typeId.hash_code());

		engine->states[(u64)typeId.hash_code()] = ptr;

		return (u64)typeId.hash_code();
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
		engine->state->onUpdate();
		engine->gui->draw();
		engine->window->display();

		engine->networkManager->update();
	}

	void tick(float deltaTime) {
		engine->state->onTick();
		engine->entityWorld.progress(deltaTime);	
	}
}

void initEngine() {
	if(engine)
		engineLog(ERROR_SEVERITY_FATAL, "Engine already initialized\n");

	engine = new Engine();
	
	// Networking
	SteamNetworkingErrMsg steamNetworkingErrMsg;
	if(!GameNetworkingSockets_Init(nullptr, steamNetworkingErrMsg))
		engineLog(ERROR_SEVERITY_FATAL, "Unable to initialize steam networking library, %s\n", (const char*)&steamNetworkingErrMsg[0]);
	engine->sockets = SteamNetworkingSockets();
	engine->util = SteamNetworkingUtils();
	engine->networkManager = std::make_shared<NetworkManager>();
	engine->entityWorldNetwork = std::make_shared<EntityWorldNetworkManager>();

	// Time
	engine->ticker.setRate(60.0f);
	engine->ticker.setFunction(impl::tick);

	// Window
	setWindow(std::make_shared<sf::RenderWindow>(sf::VideoMode(sf::Vector2u(800, 600)), "Default"));

	// State
	u64 unknownStateId = registerState<UnknownState, UnknownModule>();
	engine->state = engine->states[unknownStateId];
	
	// Physics
	engine->physicsWorld = std::make_shared<PhysicsWorld>();
	engine->physicsWorldNetwork = std::make_shared<PhysicsWorldNetworkManager>();

	// Core
	engine->entityWorld.import<CoreModule>();
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
	return *engine->state;
}

void transitionState(u64 stateId) {
	if(!impl::validState(stateId)) {
		engineLog(ERROR_SEVERITY_WARNING, "Attempted to transition to a invalid state: %u\n", stateId);
		return;
	}

	if(engine->states[stateId].get() == engine->state.get())
		return;
	
	std::shared_ptr<State> prevState = engine->state;
	std::shared_ptr<State> nextState = engine->states[stateId];

	prevState->getModule().disable();
	prevState->onLeave();
	engine->state = nextState;
	nextState->onEntry();
	nextState->getModule().enable();
}

void setWindow(std::shared_ptr<sf::RenderWindow> window) {
	engine->window = window;
	engine->gui = std::make_shared<tgui::Gui>(*engine->window);
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

void freeEngine() {
	if(!engine)
		engineLog(ERROR_SEVERITY_FATAL, "Engine has not been initialized\n");

	for(auto& pair : engine->states) {
		pair.second = nullptr;
	}

	engine->window->close();

	engine->state = nullptr;
	engine->gui = nullptr;
	engine->window = nullptr;
	engine->entityWorldNetwork = nullptr;
	engine->physicsWorld = nullptr;
	engine->physicsWorldNetwork = nullptr;
	engine->networkManager = nullptr;

	GameNetworkingSockets_Kill();

	delete engine;
}