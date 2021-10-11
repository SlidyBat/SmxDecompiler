#pragma once

#include "smx-file.h"
#include "il-cfg.h"

class CodeFixer
{
public:
	CodeFixer( SmxFile& smx ) : smx_( &smx ) {}

	void ApplyFixes( ILControlFlowGraph& cfg ) const;
private:
	void CleanStores( ILBlock& bb ) const;
	void CleanIncAndDec( ILBlock& bb ) const;
	void RemoveTmpLocalVars( ILBlock& bb ) const;

	void VisitAllNodes( ILControlFlowGraph& cfg, class ILVisitor& visitor ) const;
private:
	SmxFile* smx_;
};