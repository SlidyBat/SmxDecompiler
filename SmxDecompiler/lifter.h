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
private:
	const SmxFile* smx_;
	ILControlFlowGraph ilcfg_;

	std::vector<std::vector<ILNode*>> block_stacks_;
	std::vector<ILNode*>* stack_;
	ILNode* pri_;
	ILNode* alt_;
};