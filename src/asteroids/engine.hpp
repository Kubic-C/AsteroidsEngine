#pragma once

#include "time.hpp"
#include "core.hpp"
#include "state.hpp"

AE_NAMESPACE_BEGIN

// for when I want to change the rendering backend
typedef tgui::Gui Gui;

namespace impl {
	u64 _registerState(std::shared_ptr<State> ptr, std::type_index typeId);
	void _registerNetworkStateModule(flecs::entity module, std::type_index networkInterfaceId, u64 stateId);

	float getTickRate();
}

void init();

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
	return (u64)std::type_index(typeid(T)).hash_code();
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

void transitionState(u64 stateId, bool immediate = false);

template<typename T>
void transitionState(bool immediate = false) {
	transitionState(getStateId<T>(), immediate);
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