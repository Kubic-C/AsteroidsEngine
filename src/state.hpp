#pragma once

#include "includes.hpp"

class State {
public:
	virtual void onEntry() {}
	virtual void onLeave() {}
	virtual void onTick() {}
	virtual void onUpdate() {}

	flecs::entity getModule() const { return module; }
	void setModule(flecs::entity module) { this->module = module; }

protected:
	flecs::entity module;
};

class UnknownModule { public: UnknownModule(flecs::world& world) {} };
class UnknownState : public State {};