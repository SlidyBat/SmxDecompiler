#pragma once

#include "smx-file.h"
#include <vector>

class BasicBlock
{
public:
	BasicBlock( const cell_t* start );
	void AddTarget( BasicBlock* bb );
	void SetEnd( const cell_t* addr );

	int id() const { return id_; }
	const cell_t* start() const { return start_; }
	const cell_t* end() const { return end_; }
	size_t num_in_edges() const { return in_edges_.size(); }
	BasicBlock* in_edge( size_t index ) const { return in_edges_[index]; }
	size_t num_out_edges() const { return out_edges_.size(); }
	BasicBlock* out_edge( size_t index ) const { return out_edges_[index]; }
private:
	int id_ = 0;
	const cell_t* start_;
	const cell_t* end_;
	std::vector<BasicBlock*> in_edges_;
	std::vector<BasicBlock*> out_edges_;
};

class ControlFlowGraph
{
public:
	BasicBlock* NewBlock( const cell_t* start );
	BasicBlock* FindBlockAt( const cell_t* addr );

	size_t num_blocks() const { return blocks_.size(); }
	BasicBlock& block( size_t index ) { return blocks_[index]; }
	BasicBlock& entry() { return blocks_[0]; }
private:
	std::vector<BasicBlock> blocks_;
};