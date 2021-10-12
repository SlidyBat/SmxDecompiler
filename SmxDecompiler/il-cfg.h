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
	void Insert( size_t index, ILNode* node ) { nodes_.insert( nodes_.begin() + index, node ); }
	void Remove( ILNode* node );
	void Remove( size_t index ) { nodes_.erase( nodes_.begin() + index ); }
	void Replace( size_t index, ILNode* value ) { nodes_[index] = value; }
	void ReplaceOutEdge( ILBlock& from_block, ILBlock& to_block );
	void ReplaceInEdge( ILBlock& from_block, ILBlock& to_block );
	void RemoveOutEdge( ILBlock& block );
	void RemoveInEdge( ILBlock& block );
	void AddOutEdge( ILBlock& block );
	void AddInEdge( ILBlock& block );
	void AddToStart( ILNode* node );
	void AddToEnd( ILNode* node );
	void AddTarget( ILBlock& bb );
	ILNode* Last() { return nodes_.empty() ? nullptr : nodes_.back(); }

	cell_t pc() const { return pc_; }
	size_t id() const { return id_; }
	size_t num_nodes() const { return nodes_.size(); }
	ILNode* node( size_t index ) const { return nodes_[index]; }
	size_t num_in_edges() const { return in_edges_.size(); }
	ILBlock& in_edge( size_t index ) const { return *in_edges_[index]; }
	size_t num_out_edges() const { return out_edges_.size(); }
	ILBlock& out_edge( size_t index ) const { return *out_edges_[index]; }

	void SetImmediateDominator( ILBlock* block ) { idom_ = block; }
	ILBlock* immed_dominator() const { return idom_; }
	void SetImmediatePostDominator( ILBlock* block ) { post_idom_ = block; }
	ILBlock* immed_post_dominator() const { return post_idom_; }

	bool Dominates( ILBlock* block ) const;
	size_t NumDominators() const;

	bool IsBackEdge( size_t out_edge ) const;
	bool IsLoopHeader() const;

	bool IsVisited() const;
	void SetVisited();
private:
	friend class ILControlFlowGraph;

	const ILControlFlowGraph* cfg_;
	cell_t pc_;
	size_t id_ = 0;
	int epoch_ = 0;
	std::vector<ILNode*> nodes_;
	std::vector<ILBlock*> in_edges_;
	std::vector<ILBlock*> out_edges_;
	ILBlock* idom_ = nullptr;
	ILBlock* post_idom_ = nullptr;
};

class ILControlFlowGraph
{
public:
	void AddBlock( size_t id, cell_t pc );
	ILBlock* FindBlockAt( cell_t pc );
	ILBlock& Entry() { return blocks_[0]; }
	void Remove( ILBlock& bb );
	void RemoveMultiple( ILBlock** blocks, size_t num_blocks );

	size_t max_id() const { return blocks_.empty() ? 0 : (blocks_.size() - 1); }
	size_t num_blocks() const { return stable_blocks_.size(); }
	const ILBlock& block( size_t index ) const { return *stable_blocks_[index]; }
	ILBlock& block( size_t index ) { return *stable_blocks_[index]; }

	void SetNumArgs( int nargs ) { nargs_ = nargs; }
	int nargs() const { return nargs_; }

	int epoch() const { return epoch_; }
	void NewEpoch();

	void ComputeDominance();
	void Verify();

	ILControlFlowGraph* Next();
private:
	ILBlock* Intersect( ILBlock& b1, ILBlock& b2 );
	ILBlock* IntersectPost( ILBlock& b1, ILBlock& b2 );
	std::vector<ILBlock*> IntervalForHeader( ILBlock& header );
	size_t FindOuterTarget( const std::vector<std::vector<ILBlock*>> intervals, ILBlock* target );
private:
	int nargs_ = 0;
	std::vector<ILBlock> blocks_;
	std::vector<ILBlock*> stable_blocks_;
	int epoch_ = 0;
};