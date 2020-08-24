#pragma once

#include "smx-file.h"
#include "cfg.h"

class CfgBuilder
{
public:
	CfgBuilder( const SmxFile& smx );

	ControlFlowGraph Build( const cell_t* entry );
private:
	const cell_t* NextInstruction( const cell_t* instr ) const;
	void MarkLeaders( const cell_t* entry );
	void AddLeader( const cell_t* addr );
	bool IsLeader( const cell_t* addr ) const;
private:
	const SmxFile* smx_;
	std::vector<const cell_t*> leaders_;
	const cell_t* code_end_ = nullptr;
	ControlFlowGraph cfg_;
};