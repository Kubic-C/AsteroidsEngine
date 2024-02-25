#pragma once

#include "engine.hpp"

// avoid long namespace access ... no one likes boiler plate
AE_NAMESPACE_BEGIN
typedef json Config;

// to avoid accidently misnaming a config, we'll use constants
constexpr const char* CFG_VSYNC_ON = "vsyncOn"; // Vertical Sync ON
constexpr const char* CFG_TPS = "tps"; // Ticks per second
constexpr const char* CFG_FPS = "fps"; // Frames per second

inline void writeConfig(const Config& config, const std::string& path = "config.json") {
	std::ofstream jsonFile;

	jsonFile.open(path);
	if (!jsonFile.is_open() || jsonFile.bad()) {
		log(ERROR_SEVERITY_FATAL, "Failed to open JSON FILE(write): %s\n", path.c_str());
	}

	jsonFile << std::setw(2) << config << std::endl;

	jsonFile.close();
}

inline Config readConfig(const std::string& path = "config.json") {
	Config config;
	std::ifstream jsonFile;

	jsonFile.open(path);
	if(!jsonFile.is_open() || jsonFile.bad()) {
		return config; // empty config
	}

	config = json::parse(jsonFile);
	
	jsonFile.close();

	return config;
}

// dvalue means Default Value. If config does contain 
// key, it will add that key along with its default value;
// if it does exist, it will return that existing value.
// This is different from Config::value as ae::dvalue() modifies
// the config object and Config::value does not.
template<typename T>
T dvalue(Config& config, const char* key, T&& value) {
	if(config.contains(key))
		return config.value(key, value);
	else {
		config.emplace(key, value);
		return config.value(key, value);
	}
}

AE_NAMESPACE_END