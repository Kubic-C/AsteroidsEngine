#pragma once
#include "logging.hpp"
#include "state.hpp"
#include "engine.hpp"

extern int EntryPoint(int argc, char* argv[]);

int main(int argc, char* argv[]) {
	int returnCode = 0;

	try {
		ae::init();
		
		// if you get an error here, your main() must have argc and argv
		returnCode = EntryPoint(argc, argv);
		
		ae::free();
	} catch (ae::EngineError& error) {
		(void)error;
		ae::log(ae::ERROR_SEVERITY_WARNING, "Engine Error Occurred; Terminating");
		returnCode = -1;
	} catch(std::exception& error) {
		ae::log(ae::ERROR_SEVERITY_WARNING, "<red, bold>std::exception<reset>::what(): %s\n", error.what());
		returnCode = -1;
	} 
	
	return returnCode;
}

#define main EntryPoint