#pragma once

#include "smx-file.h"
#include "decompiler-options.h"

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