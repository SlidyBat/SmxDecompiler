#pragma once

#include "cfg.h"

class ILControlFlowGraph;
class ILNode;

class ILBlock
{
public:
	ILBlock( const ILControlFlowGraph& cfg, cell_t pc )
		:
		cfg_( &cfg ),
		pc_( pc )
	{}

	void Add( ILNode* node ) { nodes_.push_back( node ); }
	void Prepend( ILNode* node ) { nodes_.insert( nodes_.end() - 1, node ); }
	void AddTarget( ILBlock* bb );

	cell_t pc() const { return pc_; }
	size_t id() const { return id_; }
	size_t num_nodes() const { return nodes_.size(); }
	ILNode* node( size_t index ) const { return nodes_[index]; }
	size_t num_in_edges() const { return in_edges_.size(); }
	ILBlock* in_edge( size_t index ) const { return in_edges_[index]; }
	size_t num_out_edges() const { return in_edges_.size(); }
	ILBlock* out_edge( size_t index ) const { return in_edges_[index]; }

	bool IsBackEdge( size_t index ) { return out_edges_[index]->id_ < id_; }
private:
	friend class ILControlFlowGraph;

	bool IsVisited() const;
	void SetVisited();
private:
	const ILControlFlowGraph* cfg_;
	cell_t pc_;
	size_t id_ = 0;
	int epoch_ = 0;
	std::vector<ILNode*> nodes_;
	std::vector<ILBlock*> in_edges_;
	std::vector<ILBlock*> out_edges_;
};

class ILControlFlowGraph
{
public:
	void AddBlock( size_t id, cell_t pc );
	ILBlock* FindBlockAt( cell_t pc );

	size_t num_blocks() const { return blocks_.size(); }
	ILBlock& block( size_t index ) { return blocks_[index]; }

	void SetNumArgs( int nargs ) { nargs_ = nargs; }
	int nargs() const { return nargs_; }

	int epoch() const { return epoch_; }
private:
	void NewEpoch();
private:
	int nargs_ = 0;
	std::vector<ILBlock> blocks_;
	int epoch_ = 0;
};