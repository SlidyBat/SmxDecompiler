#pragma once

#include "smx-file.h"
#include "il-cfg.h"
#include "cfg.h"

class ILLocalVar;

class PcodeLifter
{
public:
	PcodeLifter( const SmxFile& smx ) : smx_( &smx ) {}

	ILControlFlowGraph Lift( const ControlFlowGraph& cfg );
private:
	void LiftBlock( BasicBlock& bb, ILBlock& ilbb );

	ILLocalVar* Push( ILNode* value );
	ILLocalVar* Pop();
	ILLocalVar* GetFrameVar( int offset );
	ILNode* GetFrameVal( int offset );
	void SetFrameVal( int offset, ILNode* val );
	class ILTempVar* MakeTemp();
private:
	const SmxFile* smx_;
	ILControlFlowGraph ilcfg_;

	struct AbstractExprStack
	{
		std::vector<ILLocalVar*> stack;
		ILNode* pri;
		ILNode* alt;
	};

	std::vector<AbstractExprStack> block_stacks_;
	size_t num_temps_;
	AbstractExprStack* expr_stack_;
};