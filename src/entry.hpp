#pragma once
#include "logging.hpp"
#include "state.hpp"
#include "engine.hpp"

extern int EntryPoint(int argc, char* argv[]);

int main(int argc, char* argv[]) {
	int returnCode = 0;

	try {
		initEngine();
		
		// if you get an error here, your main() must have argc and argv
		returnCode = EntryPoint(argc, argv);
		
		freeEngine();
	} catch (EngineError& error) {
		(void)error;
		engineLog(ERROR_SEVERITY_WARNING, "Engine Error Occurred; Terminating");
		returnCode = -1;
	} catch(std::exception& error) {
		engineLog(ERROR_SEVERITY_WARNING, "<red, bold>std::exception<reset>::what(): %s\n", error.what());
		returnCode = -1;
	} 
	
	return returnCode;
}

#define main EntryPoint