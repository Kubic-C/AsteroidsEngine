#pragma once

#include "includes.hpp"

AE_NAMESPACE_BEGIN

namespace impl {
	inline std::string trim(const std::string& str_) {
		std::string str = str_;

		str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](char x){return !std::isspace(x);}));
		str.erase(std::find_if(str.rbegin(), str.rend(), []( char ch) {
			return !std::isspace(ch);
			}).base(), str.end());
		return str;
	}
}

template<typename ... Params>
inline std::string formatString(const std::string& format, Params&& ... args) {

    int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1;
    auto size = static_cast<size_t>( size_s );
    std::unique_ptr<char[]> buf( new char[ size ] );
    std::snprintf( buf.get(), size, format.c_str(), args ... );

    return { buf.get(), buf.get() + size - 1 };
}

enum ErrorSeverity {
	ERROR_SEVERITY_NONE,
	ERROR_SEVERITY_WARNING,
	ERROR_SEVERITY_FATAL,
};

class EngineError : std::runtime_error {
public:
	explicit EngineError(const std::string& str)
		: runtime_error(str) {
	}
};

inline class Logger {
public:
	static constexpr const char* ANSI_RESET = "\u001B[0m";
	static constexpr const char* ANSI_BOLD = "\u001B[1m";
	static constexpr const char* ANSI_ITALIC = "\u001B[3m";
	static constexpr const char* ANSI_BLACK = "\u001B[30m";
	static constexpr const char* ANSI_RED = "\u001B[31m";
	static constexpr const char* ANSI_GREEN = "\u001B[32m";
	static constexpr const char* ANSI_YELLOW = "\u001B[33m";
	static constexpr const char* ANSI_BLUE = "\u001B[34m";
	static constexpr const char* ANSI_PURPLE = "\u001B[35m";
	static constexpr const char* ANSI_CYAN = "\u001B[36m";
	static constexpr const char* ANSI_WHITE = "\u001B[37m";
	
	static constexpr char START_DECORATOR = '<';
	static constexpr const char* DECORATOR_BOLD = "bold";
	static constexpr const char* DECORATOR_ITALIC = "it";
	static constexpr const char* DECORATOR_BLACK = "black";
	static constexpr const char* DECORATOR_RED = "red";
	static constexpr const char* DECORATOR_GREEN = "green";
	static constexpr const char* DECORATOR_YELLOW = "yellow";
	static constexpr const char* DECORATOR_BLUE = "blue";
	static constexpr const char* DECORATOR_PURPLE = "purple";
	static constexpr const char* DECORATOR_CYAN = "cyan";
	static constexpr const char* DECORATOR_WHITE = "white";
	static constexpr const char* DECORATOR_RESET = "reset";
	static constexpr char SEPERATOR_DECORATOR = ',';
	static constexpr char END_DECORATOR = '>';

	Logger() {
		decoratorMap[DECORATOR_RESET]  = ANSI_RESET;
		decoratorMap[DECORATOR_BOLD]   = ANSI_BOLD;
		decoratorMap[DECORATOR_ITALIC] = ANSI_ITALIC;
		decoratorMap[DECORATOR_BLACK]  = ANSI_BLACK;
		decoratorMap[DECORATOR_RED]    = ANSI_RED;
		decoratorMap[DECORATOR_GREEN ] = ANSI_GREEN;
		decoratorMap[DECORATOR_YELLOW] = ANSI_YELLOW;
		decoratorMap[DECORATOR_BLUE]   = ANSI_BLUE;
		decoratorMap[DECORATOR_PURPLE] = ANSI_PURPLE;
		decoratorMap[DECORATOR_CYAN]   = ANSI_CYAN;
		decoratorMap[DECORATOR_WHITE]  = ANSI_WHITE;
		decoratorMap[DECORATOR_RESET]  = ANSI_RESET;

		logFile.open(std::string("log") + std::to_string(time(nullptr)) + ".txt");
	}

	~Logger() {
		if(logFile.is_open())
			logFile.close();
	}

	// Will simply print to the console
    template<typename ... Params>
	void operator()(const std::string& format, Params&& ... args) {
		_log(ERROR_SEVERITY_NONE, format, args...);
	}

	// Will print to the console but may throw an exception depending on the severity level
    template<typename ... Params>
	void operator()(ErrorSeverity severity, const std::string& format, Params&& ... args) {
		_log(severity, format, args...);
	}

private:
	int parseDecoratorSet(std::string& str, int start, bool deleteDecorator = false) {
		std::string decorations = "";
		
		int startOfDecoration = start + 1;
		int i = start;
		for(; i < str.size() && str[i] != END_DECORATOR; i++) {
			if(str[i] == SEPERATOR_DECORATOR) {
				std::string decorationKey = impl::trim(str.substr(startOfDecoration, i - startOfDecoration));
				if(decoratorMap.find(decorationKey) != decoratorMap.end())
					decorations += decoratorMap[decorationKey];
					
				startOfDecoration = i + 1;
			}
		}

		std::string decorationKey = impl::trim(str.substr(startOfDecoration, i - startOfDecoration));
		if (decoratorMap.find(decorationKey) != decoratorMap.end())
			decorations += decoratorMap[decorationKey];

		str.erase(str.begin() + start, str.begin() + i + 1);
		if(!deleteDecorator) {
			str.insert(start, decorations);
			return start + (int)decorations.size() - 1;
		}

		return start - 1;
	}

	std::string parseForDecorators(const std::string& format, bool deleteDecorators = false) {
		std::string newFormat = format;

		for(int i = 0; i < newFormat.size(); i++) {
			if(newFormat[i] == START_DECORATOR) {
				i = parseDecoratorSet(newFormat, i, deleteDecorators);
			}
		}

		return newFormat;
	}

    template<typename ... Params>
	void _log(ErrorSeverity severity, const std::string& format, Params&& ... args) {
		std::string output(format);
		
		if(severity == ERROR_SEVERITY_FATAL)
			output.insert(0, "<red, bold>Fatal Error: <reset>");
		else if(severity == ERROR_SEVERITY_WARNING)
			output.insert(0, "<yellow>Warning: <reset>");
		if(logFile.is_open())
			logFile << formatString(parseForDecorators(output, true), args...);
		output = formatString(parseForDecorators(output), args...);

		std::cout << output;

		handleError(severity, output);
	}

	void handleError(ErrorSeverity severity, std::string& errorString) {
		if(severity == ERROR_SEVERITY_FATAL)
			throw EngineError(errorString);
	}

private:
	std::ofstream logFile;
	std::map<std::string, std::string> decoratorMap;
} log;

#ifndef NDEBUG
// if expr is true, log the warning
#define debugWarning(expr, format, ...) \
	((expr) ? log(::ae::ERROR_SEVERITY_WARNING, format, __VA_ARGS__) : (void)0 )
#else
#define debugWarning(format, ...)
#endif

AE_NAMESPACE_END
