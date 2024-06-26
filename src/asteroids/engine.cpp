#include "engine.hpp"
#include "core.hpp"
#include "network.hpp"

AE_NAMESPACE_BEGIN

u64 generateNewStateId() {
	static u64 counter = 1;
	return ++counter;
}

struct StateInfo {
	std::shared_ptr<State> state = nullptr;
	std::unordered_map<std::type_index, flecs::entity> networkModules;
};

/* for internal use only */
struct Engine {
	// ORDERED BY INITIALIZATION, DO NOT CHANGE THE ORDER- thank you

	std::function<void(Config& config)> applyConfigCallback;
	Config config;
	std::shared_ptr<PhysicsWorld> physicsWorld;
	flecs::world entityWorld;
	ISteamNetworkingSockets* sockets;
	ISteamNetworkingUtils* util;
	std::shared_ptr<NetworkManager> networkManager;
	std::shared_ptr<NetworkStateManager> networkStateManager;
	Ticker<void(float)> ticker;
	std::function<void()> updateCallback;
	std::shared_ptr<sf::RenderWindow> window;
	std::shared_ptr<tgui::Gui> gui;
	impl::FastMap<u64, u64> stateIdTranslationTable; // this used to transform a State type hash code into a controllable unique type ID
	impl::FastMap<u64, StateInfo> states;
	u64 activeState;
	std::queue<u64> nextActiveState;
	u64 lastState; // the state in the last tick
	u64 currentTick;
};

Engine* engine = nullptr; 

namespace impl {
	bool validStateTypeId(u64 hashCode) {
		return engine->stateIdTranslationTable.find(hashCode) != engine->stateIdTranslationTable.end();
	}

	bool validState(u64 id) {
		return engine->states.find(id) != engine->states.end();
	}

	float getTickRate() {
		return engine->ticker.getRate();
	}

	u64 _registerState(std::shared_ptr<State> ptr, std::type_index typeId) {
		if(typeId.hash_code() > UINT64_MAX)
			log(ERROR_SEVERITY_FATAL, "Hashcode of state surpassed max u64\n");

		if(validStateTypeId((u64)typeId.hash_code()))
			log(ERROR_SEVERITY_FATAL, "State already registered: %s: %llu\n", typeId.name(), (u64)typeId.hash_code());

		engine->stateIdTranslationTable[(u64)typeId.hash_code()] = generateNewStateId();
		engine->states[engine->stateIdTranslationTable[(u64)typeId.hash_code()]].state = std::move(ptr);

		return engine->stateIdTranslationTable[(u64)typeId.hash_code()];
	}

	void _registerNetworkStateModule(flecs::entity module, std::type_index networkInterfaceId, u64 stateId) {
		if(!validState(stateId))
			log(ERROR_SEVERITY_FATAL, "Cannot register Network State Module if the state is not already registered");

		engine->states[stateId].networkModules[networkInterfaceId] = module;
	}

	void transitionState(u64 stateId, bool force) {
		if (!impl::validState(stateId)) {
			log(ERROR_SEVERITY_WARNING, "Attempted to transition to a invalid state: %u\n", stateId);
			return;
		}

		if (stateId == engine->activeState && !force)
			return;

		StateInfo& prevState = engine->states[engine->activeState];
		StateInfo& nextState = engine->states[stateId];

		prevState.state->getModule().disable();
		prevState.state->onLeave();
		engine->activeState = stateId;
		nextState.state->onEntry();
		nextState.state->getModule().enable();

		for(auto pair : prevState.networkModules) {
			pair.second.disable(); // disable all network modules
		}

		// enable NetworkStateModules
		NetworkManager& networkManager = *engine->networkManager;
		if (networkManager.hasNetworkInterface()) {
			NetworkInterface& interface = networkManager.getNetworkInterface();
			auto id = std::type_index(typeid(interface));

			if (nextState.networkModules.find(id) != nextState.networkModules.end()) {
				nextState.networkModules[id].enable();
			}

			engine->networkStateManager->userStateChanged();
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

	impl::FastMap<u64, u64>& getStateIdTranslationTable() {
		return engine->stateIdTranslationTable;
	}

	void update() {
		sf::Event event;
		while(event = engine->window->pollEvent()) {
            if(event.is<sf::Event::Closed>())
                engine->entityWorld.quit();

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

		if(!engine->nextActiveState.empty()) {
			::ae::impl::transitionState(engine->nextActiveState.front(), false);
			engine->nextActiveState.pop();
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

	// Read in configs
	Config inConfig = readConfig();
	if(inConfig.empty()) {
		writeConfig(json::object());
	}

	inConfig = readConfig();
	engine->applyConfigCallback = nullptr;

	// Networking
	SteamNetworkingErrMsg steamNetworkingErrMsg;
	if(!GameNetworkingSockets_Init(nullptr, steamNetworkingErrMsg))
		log(ERROR_SEVERITY_FATAL, "Unable to initialize steam networking library, %s\n", (const char*)&steamNetworkingErrMsg[0]);
	engine->sockets = SteamNetworkingSockets();
	engine->util = SteamNetworkingUtils();
	engine->networkManager = std::make_shared<NetworkManager>();
	engine->networkStateManager = std::make_shared<NetworkStateManager>();
	CoreModule::registerCore();

	// For testing the network :)
	float FakePacketLoss_Send      = 0;
	float FakePacketLoss_Recv      = 0;
	i32   FakePacketLag_Send       = 0;
	i32   FakePacketLag_Recv       = 0;
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

	// Core
	engine->entityWorld.import<CoreModule>();

	// State
	u64 unknownStateId = registerState<UnknownState, UnknownModule>();
	engine->activeState = unknownStateId;

	// Ticking
	engine->currentTick = 0;

	// And set the config
	applyConfig(inConfig);
}

Config& getConfig() {
	return engine->config;
}

void setConfigApplyCallback(std::function<void(Config& config)> callback) {
	engine->applyConfigCallback = callback;
}

void applyConfig(Config newConfig) {
	setFps((u32)dvalue<i64>(newConfig, CFG_FPS, 60));
	setTps((float)dvalue<double>(newConfig, CFG_TPS, 60.0));
	getWindow().setVerticalSyncEnabled(dvalue<bool>(newConfig, CFG_VSYNC_ON, true));

	if(engine->applyConfigCallback)
		engine->applyConfigCallback(newConfig);

	engine->config = std::move(newConfig);
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

PhysicsWorld& getPhysicsWorld() {
	return *engine->physicsWorld;
}

NetworkStateManager& getNetworkStateManager() {
	return *engine->networkStateManager;
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

void transitionState(u64 id, bool immediate, bool force) {
	if(immediate && !getEntityWorld().is_deferred()) {
		impl::transitionState(id, force);
		return;
	}

	engine->nextActiveState.push(id);
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

void setFps(u32 fps) {
	engine->window->setFramerateLimit(fps);
}

void setTps(float tps) {
	if(tps == 0.0f)
		ae::log(ERROR_SEVERITY_FATAL, "Attempt to set TPS to 0.0f\n");

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
	writeConfig(engine->config);

	if(!engine)
		log(ERROR_SEVERITY_FATAL, "Engine has not been initialized\n");

	delete engine;

	GameNetworkingSockets_Kill();
}

AE_NAMESPACE_END