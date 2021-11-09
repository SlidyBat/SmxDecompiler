#pragma once

enum class StringDetectType
{
	NONE,       // Won't attempt to detect strings
	AGGRESSIVE, // Replace constants that possibly point to string with string itself
	COMMENT,    // Places comment next to the constant of what string it could be
};

struct DecompilerOptions
{
	bool print_globals;
	bool print_il;
	bool print_assembly;
	const char* function;
	StringDetectType string_detect;
};