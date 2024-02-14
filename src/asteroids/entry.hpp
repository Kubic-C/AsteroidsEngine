#pragma once
#include "logging.hpp"
#include "engine.hpp"

extern int EntryPoint(int argc, char* argv[]);

// in the case that entry.hpp is already included
#undef main

int main(int argc, char* argv[]);

#define main EntryPoint