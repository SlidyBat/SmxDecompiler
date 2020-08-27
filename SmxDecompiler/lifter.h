#pragma once

#include "smx-file.h"
#include "il-cfg.h"
#include "cfg.h"

class PcodeLifter
{
public:
	PcodeLifter( const SmxFile& smx ) : smx_( &smx ) {}

	ILControlFlowGraph Lift( const ControlFlowGraph& cfg );
private:
	void LiftBlock( BasicBlock& bb, ILBlock& ilbb );

	void Push( ILNode* node );
	ILNode* Pop();
	ILNode* GetFrameValue( int offset );
	class ILTempVar* MakeTemp();
private:
	const SmxFile* smx_;
	ILControlFlowGraph ilcfg_;

	struct AbstractExprStack
	{
		std::vector<ILNode*> stack;
		ILNode* pri;
		ILNode* alt;
	};

	std::vector<AbstractExprStack> block_stacks_;
	size_t num_temps_;
	AbstractExprStack* expr_stack_;
};