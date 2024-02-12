#pragma once

#include "time.hpp"
#include "core.hpp"
#include "state.hpp"

AE_NAMESPACE_BEGIN

// for when I want to change the rendering backend
typedef tgui::Gui Gui;

namespace impl {
	u64 _registerState(std::shared_ptr<State> ptr, std::type_index typeId);
}

void init();

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

template<typename T>
T& getCurrentState() {
	return dynamic_cast<T&>(getCurrentState());
}

void transitionState(u64 stateId);

template<typename T>
void transitionState() {
	transitionState(getStateId<T>());
}

void setWindow(std::shared_ptr<sf::RenderWindow> window);

sf::RenderWindow& getWindow();

void setFps(u32 fps);

void setTps(float tps);

void setUpdateCallback(std::function<void()> callback);
void mainLoop();

void free();

AE_NAMESPACE_END