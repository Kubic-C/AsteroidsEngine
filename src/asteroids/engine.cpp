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
	std::shared_ptr<NetworkSnapshotManager> networkSnaphots;
	std::unordered_map<u64, StateInfo> states;
	u64 activeState;
	u64 nextActiveState;
	u64 lastState; // the state in the last tick
	u64 currentTick;
};

Engine* engine = nullptr; 

namespace impl {
	bool validState(u64 id) {
		return engine->states.find(id) != engine->states.end();
	}

	float getTickRate() {
		return engine->ticker.getRate();
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

	void transitionState(u64 stateId) {
		if (!impl::validState(stateId)) {
			log(ERROR_SEVERITY_WARNING, "Attempted to transition to a invalid state: %u\n", stateId);
			return;
		}

		if (stateId == engine->activeState)
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
		if (networkManager.hasNetworkInterface()) {
			NetworkInterface& interface = networkManager.getNetworkInterface();
			std::type_index id = std::type_index(typeid(interface));

			if (prevState.networkModules.find(id) != prevState.networkModules.end()) {
				prevState.networkModules[id].disable();
			}
			if (nextState.networkModules.find(id) != nextState.networkModules.end()) {
				nextState.networkModules[id].enable();
			}

			// Some interface network code such as sync updates may need to know if state has changed
			interface._internalUpdate();
			interface.update();
		}
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

		engine->networkManager->beginTick();
		engine->entityWorld.progress(deltaTime);
		engine->networkManager->endTick();
		
		engine->currentTick++;
		engine->lastState = engine->activeState;

		if(engine->nextActiveState != UINT64_MAX) {
			::ae::impl::transitionState(engine->nextActiveState);
			engine->nextActiveState = UINT64_MAX;
		}
	}
}

void setGlobalNetworkingConfig(ESteamNetworkingConfigValue config, ESteamNetworkingConfigDataType type, const void* data) {
	engine->util->SetConfigValue(config, k_ESteamNetworkingConfig_Global, 0, type, data);
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

	int32_t maxMessageSize = 1000000; // 1 mega byte
	engine->util->SetConfigValue(k_ESteamNetworkingConfig_SendBufferSize, k_ESteamNetworkingConfig_Global, 0,  k_ESteamNetworkingConfig_Int32, &maxMessageSize);

	// For testing the network :)
	float FakePacketLoss_Send      = 20;
	float FakePacketLoss_Recv      = 00;
	i32   FakePacketLag_Send       = 10;
	i32   FakePacketLag_Recv       = 10;
	float FakePacketReorder_Send   = 0;
	float FakePacketReorder_Recv   = 0;
	i32   FakePacketReorder_Time   = 0;
	float FakePacketDup_Send       = 0;
	float FakePacketDup_Recv       = 0;
	i32   FakePacketDup_TimeMax    = 0;
	i32   PacketTraceMaxBytes      = 0;
	i32   FakeRateLimit_Send_Rate  = 0;
	i32   FakeRateLimit_Send_Burst = 0;
	i32   FakeRateLimit_Recv_Rate  = 0;
	i32   FakeRateLimit_Recv_Burst = 0;
	setGlobalNetworkingConfig(k_ESteamNetworkingConfig_FakePacketLoss_Send,      k_ESteamNetworkingConfig_Float, &FakePacketLoss_Send);
	setGlobalNetworkingConfig(k_ESteamNetworkingConfig_FakePacketLoss_Recv,      k_ESteamNetworkingConfig_Float, &FakePacketLoss_Recv);
	setGlobalNetworkingConfig(k_ESteamNetworkingConfig_FakePacketLag_Send,       k_ESteamNetworkingConfig_Int32, &FakePacketLag_Send);
	setGlobalNetworkingConfig(k_ESteamNetworkingConfig_FakePacketLag_Recv,       k_ESteamNetworkingConfig_Int32, &FakePacketLag_Recv);
	setGlobalNetworkingConfig(k_ESteamNetworkingConfig_FakePacketReorder_Send,   k_ESteamNetworkingConfig_Float, &FakePacketReorder_Send);
	setGlobalNetworkingConfig(k_ESteamNetworkingConfig_FakePacketReorder_Recv,   k_ESteamNetworkingConfig_Float, &FakePacketReorder_Recv);
	setGlobalNetworkingConfig(k_ESteamNetworkingConfig_FakePacketReorder_Time,   k_ESteamNetworkingConfig_Int32, &FakePacketReorder_Time);
	setGlobalNetworkingConfig(k_ESteamNetworkingConfig_FakePacketDup_Send,       k_ESteamNetworkingConfig_Float, &FakePacketDup_Send);
	setGlobalNetworkingConfig(k_ESteamNetworkingConfig_FakePacketDup_Recv,       k_ESteamNetworkingConfig_Float, &FakePacketDup_Recv);
	setGlobalNetworkingConfig(k_ESteamNetworkingConfig_FakePacketDup_TimeMax,    k_ESteamNetworkingConfig_Int32, &FakePacketDup_TimeMax);
	setGlobalNetworkingConfig(k_ESteamNetworkingConfig_PacketTraceMaxBytes,      k_ESteamNetworkingConfig_Int32, &PacketTraceMaxBytes);
	setGlobalNetworkingConfig(k_ESteamNetworkingConfig_FakeRateLimit_Send_Rate,  k_ESteamNetworkingConfig_Int32, &FakeRateLimit_Send_Rate);
	setGlobalNetworkingConfig(k_ESteamNetworkingConfig_FakeRateLimit_Send_Burst, k_ESteamNetworkingConfig_Int32, &FakeRateLimit_Send_Burst);
	setGlobalNetworkingConfig(k_ESteamNetworkingConfig_FakeRateLimit_Recv_Rate,  k_ESteamNetworkingConfig_Int32, &FakeRateLimit_Recv_Rate);
	setGlobalNetworkingConfig(k_ESteamNetworkingConfig_FakeRateLimit_Recv_Burst, k_ESteamNetworkingConfig_Int32, &FakeRateLimit_Recv_Burst);

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

	// Ticking
	engine->currentTick = 0;
}

u64 getCurrentTick() {
	return engine->currentTick;
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

void transitionState(u64 id, bool immediate) {
	if(immediate && !getEntityWorld().is_deferred()) {
		impl::transitionState(id);
		return;
	}

	engine->nextActiveState = id;
}

bool hasStateChanged() {
	return engine->lastState != engine->activeState;
}

void setWindow(std::shared_ptr<sf::RenderWindow> window) {
	engine->window = window;
	engine->gui = std::make_shared<tgui::Gui>();
	engine->gui->setWindow(*engine->window);
}

sf::RenderWindow& getWindow() {
	return *engine->window;
}

void enableSnapshots() {
	assert(!engine->networkSnaphots);

	engine->networkSnaphots = std::make_shared<NetworkSnapshotManager>();
}

void disableSnapshots() {
	engine->networkSnaphots = nullptr;
}

NetworkSnapshotManager& getNetworkSnapshotManager() {
	assert(engine->networkSnaphots && "Must have snapshots enabled");
	return *engine->networkSnaphots;
}

bool isSnapshotsEnabled() {
	return engine->networkSnaphots != nullptr;
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