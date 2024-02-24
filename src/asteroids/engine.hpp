#pragma once

#include "logging.hpp"
#include "physics.hpp"
#include "time.hpp"
#include "state.hpp"
#include "config.hpp"

AE_NAMESPACE_BEGIN

// for when I want to change the rendering backend
typedef tgui::Gui Gui;

namespace impl {
	u64 _registerState(std::shared_ptr<State> ptr, std::type_index typeId);
	void _registerNetworkStateModule(flecs::entity module, std::type_index networkInterfaceId, u64 stateId);

	float getTickRate();

	FastMap<u64, u64>& getStateIdTranslationTable();
}

void init();

Config& getConfig();

template<typename T>
T& getConfigValue(const std::string& name) {
	if(!getConfig().contains(name))
		log(ERROR_SEVERITY_FATAL, "Config: %s does not exist\n", name.c_str());

	return getConfig().at(name).get_ref<T>();
}

// whenever applyConfig() is called, this will be called as well
void setConfigApplyCallback(std::function<void(Config& config)> callback);
void applyConfig(Config newConfig = readConfig());

u64 getCurrentTick();

Gui& getGui();

flecs::world& getEntityWorld();

PhysicsWorld& getPhysicsWorld();

template<typename T, typename TModule = UnknownModule>
u64 registerState() {
	std::shared_ptr<T> newState = std::make_shared<T>();

	newState->setModule(getEntityWorld().import<TModule>());
	newState->getModule().disable();

	return impl::_registerState(std::dynamic_pointer_cast<State>(newState), std::type_index(typeid(T)));
}

template<typename T>
u64 getStateId() {
	return impl::getStateIdTranslationTable()[(u64)std::type_index(typeid(T)).hash_code()];
}

State& getCurrentState();

u64 getCurrentStateId();

template<typename T>
T& getCurrentState() {
	return dynamic_cast<T&>(getCurrentState());
}

// when the NetworkInterfaceType is currently being used
// and the state type is currently being used, make ModuleType active
template<typename NetworkInterfaceType, typename StateType, typename ModuleType>
void registerNetworkInterfaceStateModule() {
	flecs::world& world = getEntityWorld();
	flecs::entity module = world.import<ModuleType>();

	module.disable();
	impl::_registerNetworkStateModule(module, std::type_index(typeid(NetworkInterfaceType)), getStateId<StateType>());
}

void transitionState(u64 stateId, bool immediate = false, bool force = false);

template<typename T>
void transitionState(bool immediate = false, bool force = false) {
	transitionState(getStateId<T>(), immediate, force);
}

bool hasStateChanged();

void setWindow(std::shared_ptr<sf::RenderWindow> window);

sf::RenderWindow& getWindow();

void setFps(u32 fps);

void setTps(float tps);

void setUpdateCallback(std::function<void()> callback);
void mainLoop();

void free();

AE_NAMESPACE_END