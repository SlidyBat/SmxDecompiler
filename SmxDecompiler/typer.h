#pragma once

#include "il.h"
#include "il-cfg.h"

class Typer
{
public:
	Typer( SmxFile& smx ) : smx_( &smx ) {}

	void PopulateTypes( ILControlFlowGraph& cfg );
	void PropagateTypes( ILControlFlowGraph& cfg );
private:
	void FillSmxVars( ILControlFlowGraph& cfg, const SmxFunction* func );
	void VisitAllNodes( ILControlFlowGraph& cfg, ILVisitor& visitor );
private:
	SmxFile* smx_;
};