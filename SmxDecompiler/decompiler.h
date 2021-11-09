#pragma once

#include "smx-file.h"

struct DecompilerOptions
{
	bool print_globals;
	bool print_il;
	bool print_assembly;
	const char* function;
};

class Decompiler
{
public:
	Decompiler( SmxFile& smx, const DecompilerOptions& options );

	void Print();

private:
	void DiscoverFunctions( class ControlFlowGraph& cfg );

private:
	SmxFile* smx_;
	DecompilerOptions options_;
};