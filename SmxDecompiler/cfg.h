#pragma once

#include "smx-file.h"
#include <vector>

class ControlFlowGraph;

class BasicBlock
{
public:
	BasicBlock( const ControlFlowGraph& cfg, const cell_t* start );
	void AddTarget( BasicBlock* bb );
	void SetEnd( const cell_t* addr );

	bool Contains( const cell_t* addr ) const;

	// Number in RPO
	size_t id() const { return id_; }
	const cell_t* start() const { return start_; }
	const cell_t* end() const { return end_; }
	size_t num_in_edges() const { return in_edges_.size(); }
	BasicBlock* in_edge( size_t index ) const { return in_edges_[index]; }
	size_t num_out_edges() const { return out_edges_.size(); }
	BasicBlock* out_edge( size_t index ) const { return out_edges_[index]; }

	bool IsBackEdge( size_t index ) const;
private:
	bool IsVisited() const;
	void SetVisited();
private:
	friend class ControlFlowGraph;

	const ControlFlowGraph* cfg_;
	size_t id_ = 0;
	int epoch_ = 0;
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
	BasicBlock& EntryBlock() { return blocks_[0]; }

	void SetNumArgs( int nargs ) { nargs_ = nargs; }
	int nargs() const { return nargs_; }

	int epoch() const { return epoch_; }

	size_t num_blocks() const { return ordered_blocks_.size(); }
	BasicBlock& block( size_t index ) const { return *ordered_blocks_[index]; }
	void Remove( size_t block_index );

	void ComputeOrdering();
private:
	size_t VisitPostOrderAndSetId( BasicBlock& bb, size_t po_number );

	void NewEpoch() { epoch_++; }
private:
	int nargs_ = 0;
	std::vector<BasicBlock> blocks_;
	// Blocks ordered in reverse post-order
	// In separate container so that pointers to blocks are never invalidated
	std::vector<BasicBlock*> ordered_blocks_;
	int epoch_ = 0;
};