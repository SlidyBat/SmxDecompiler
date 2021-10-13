#include "lifter.h"

#include "il.h"
#include "smx-opcodes.h"
#include <cassert>

ILControlFlowGraph* PcodeLifter::Lift( const ControlFlowGraph& cfg )
{
	num_temps_ = 0;
	heap_addr_ = 0;

	ilcfg_ = new ILControlFlowGraph;
	ilcfg_->SetNumArgs( cfg.nargs() );

	for( size_t i = 0; i < cfg.num_blocks(); i++ )
	{
		BasicBlock& bb = cfg.block( i );
		ilcfg_->AddBlock( bb.id(), (cell_t)((intptr_t)bb.start() - (intptr_t)smx_->code()) );
	}

	block_stacks_.resize( ilcfg_->num_blocks() );
	for( size_t i = 0; i < cfg.num_blocks(); i++ )
	{
		BasicBlock& bb = cfg.block( i );
		ILBlock& ilbb = ilcfg_->block( i );
		LiftBlock( bb, ilbb );
	}

	ilcfg_->ComputeDominance();

	for( size_t i = 0; i < cfg.num_blocks(); i++ )
	{
		ILBlock& ilbb = ilcfg_->block( i );
		CleanCalls( ilbb );
	}
	for( size_t i = 0; i < cfg.num_blocks(); i++ )
	{
		ILBlock& ilbb = ilcfg_->block( i );
		PruneVarsInBlock( ilbb );
	}
	for( size_t i = 0; i < cfg.num_blocks(); i++ )
	{
		ILBlock& ilbb = ilcfg_->block( i );
		MovePhis( ilbb );
	}

	CompoundConditions();

	// All the operations above may have moved around some edges and removed basic blocks
	// Recompute the dominance for the graph again in case there are stale references there
	ilcfg_->ComputeDominance();

	return ilcfg_;
}

void PcodeLifter::LiftBlock( BasicBlock& bb, ILBlock& ilbb )
{
	expr_stack_ = &block_stacks_[ilbb.id()];
	ILNode*& pri = expr_stack_->pri;
	ILNode*& alt = expr_stack_->alt;

	for( size_t i = 0; i < bb.num_out_edges(); i++ )
	{
		BasicBlock* out = bb.out_edge( i );
		ILBlock& ilout = ilcfg_->block( out->id() );
		ilbb.AddTarget( ilout );
	}

	for( size_t i = 0; i < bb.num_in_edges(); i++ )
	{
		BasicBlock* in = bb.in_edge( i );

		// No back edges
		if( in->id() >= bb.id() )
		{
			continue;
		}

		if( expr_stack_->stack.empty() )
		{
			*expr_stack_ = block_stacks_[in->id()];
		}
		else
		{
			auto join_reg = [&]( ILNode*& reg, ILNode* value ) {
				ILPhi* phi = dynamic_cast<ILPhi*>(reg);
				if( !phi )
				{
					phi = new ILPhi;
					phi->AddInput( reg );
					reg = phi;
				}
				phi->AddInput( value );
			};
			join_reg( pri, block_stacks_[in->id()].pri );
			join_reg( alt, block_stacks_[in->id()].alt );
		}
	}

	if( ILPhi* phi = dynamic_cast<ILPhi*>(pri) )
	{
		ILTempVar* var = MakeTemp( phi );
		ilbb.Add( var );
		pri = var;
	}
	if( ILPhi* phi = dynamic_cast<ILPhi*>(alt) )
	{
		ILTempVar* var = MakeTemp( phi );
		ilbb.Add( var );
		alt = var ;
	}

	const cell_t* instr = bb.start();
	while( instr < bb.end() )
	{
		auto op = (SmxOpcode)instr[0];
		const cell_t* params = instr + 1;
		const auto& info = SmxInstrInfo::Get( op );
		const cell_t* next_instr = instr + info.num_params + 1;

		auto handle_jmp = [&]( ILBinary* cmp ) {
			ILBlock* true_branch = ilcfg_->FindBlockAt( params[0] );
			ILBlock* false_branch = ilcfg_->FindBlockAt( (cell_t)((intptr_t)next_instr - (intptr_t)smx_->code()) );
			assert( true_branch && false_branch );
			ilbb.Add( new ILJumpCond( cmp, true_branch, false_branch ) );
		};

		switch( op )
		{
			case SMX_OP_PROC:
			{
				for( int i = 0; i < ilcfg_->nargs(); i++ )
				{
					Push( nullptr );
				}

				Push( nullptr ); // Number of args
				Push( nullptr ); // Frame pointer
				Push( nullptr ); // Heap pointer

				break;
			}

			case SMX_OP_STACK:
			{
				cell_t amount = params[0];
				if( amount < 0 )
				{
					for( cell_t i = 0; i < -amount / 4; i++ )
					{
						ilbb.Add( Push( nullptr ) );
					}
				}
				else
				{
					for( cell_t i = 0; i < amount / 4; i++ )
					{
						Pop();
					}
				}
				break;
			}
			case SMX_OP_HEAP:
			{
				cell_t size = params[0];
				cell_t addr = heap_addr_;
				alt = new ILHeapVar( heap_addr_, size );
				heap_addr_ += size;
				ilbb.Add( alt );
				break;
			}
			case SMX_OP_FILL:
				break;

			case SMX_OP_PUSH:
			case SMX_OP_PUSH2:
			case SMX_OP_PUSH3:
			case SMX_OP_PUSH4:
			case SMX_OP_PUSH5:
			{
				size_t nvals = 1;
				if( op >= SMX_OP_PUSH2 )
				{
					nvals = ( op - SMX_OP_PUSH2 ) / 4 + 2;
				}

				for( size_t i = 0; i < nvals; i++ )
				{
					ilbb.Add( Push( new ILLoad( new ILGlobalVar( params[i] ) ) ) );
				}

				break;
			}

			case SMX_OP_PUSH_S:
			case SMX_OP_PUSH2_S:
			case SMX_OP_PUSH3_S:
			case SMX_OP_PUSH4_S:
			case SMX_OP_PUSH5_S:
			{
				size_t nvals = 1;
				if( op >= SMX_OP_PUSH2_S )
				{
					nvals = ( op - SMX_OP_PUSH2_S ) / 4 + 2;
				}

				for( size_t i = 0; i < nvals; i++ )
				{
					ilbb.Add( Push( new ILLoad( GetFrameVar( params[i] ) ) ) );
				}

				break;
			}

			case SMX_OP_PUSH_C:
			case SMX_OP_PUSH2_C:
			case SMX_OP_PUSH3_C:
			case SMX_OP_PUSH4_C:
			case SMX_OP_PUSH5_C:
			{
				size_t nvals = 1;
				if( op >= SMX_OP_PUSH2_C )
				{
					nvals = ( op - SMX_OP_PUSH2_C ) / 4 + 2;
				}

				for( size_t i = 0; i < nvals; i++ )
				{
					ilbb.Add( Push( new ILConst( params[i] ) ) );
				}

				break;
			}

			case SMX_OP_PUSH_ADR:
			case SMX_OP_PUSH2_ADR:
			case SMX_OP_PUSH3_ADR:
			case SMX_OP_PUSH4_ADR:
			case SMX_OP_PUSH5_ADR:
			{
				size_t nvals = 1;
				if( op >= SMX_OP_PUSH2_ADR )
				{
					nvals = ( op - SMX_OP_PUSH2_ADR ) / 4 + 2;
				}

				for( size_t i = 0; i < nvals; i++ )
				{
					ilbb.Add( Push( GetFrameVar( params[i] ) ) );
				}

				break;
			}

			case SMX_OP_PUSH_PRI:
				ilbb.Add( Push( pri ) );
				break;
			case SMX_OP_PUSH_ALT:
				ilbb.Add( Push( alt ) );
				break;

			case SMX_OP_POP_PRI:
				pri = PopValue();
				break;
			case SMX_OP_POP_ALT:
				alt = PopValue();
				break;

			case SMX_OP_CONST_PRI:
				pri = new ILConst( params[0] );
				break;
			case SMX_OP_CONST_ALT:
				alt = new ILConst( params[0] );
				break;
			case SMX_OP_CONST:
				ilbb.Add( new ILStore( new ILGlobalVar( params[0] ), new ILConst( params[1] ) ) );
				break;
			case SMX_OP_CONST_S:
				ilbb.Add( new ILStore( GetFrameVar( params[0] ), new ILConst( params[1] ) ) );
				break;

			case SMX_OP_LOAD_PRI:
				pri = new ILLoad( new ILGlobalVar( params[0] ) );
				break;
			case SMX_OP_LOAD_ALT:
				alt = new ILLoad( new ILGlobalVar( params[0] ) );
				break;
			case SMX_OP_LOAD_BOTH:
				pri = new ILLoad( new ILGlobalVar( params[0] ) );
				alt = new ILLoad( new ILGlobalVar( params[1] ) );
				break;
			case SMX_OP_LOAD_S_PRI:
				pri = new ILLoad( GetFrameVar( params[0] ) );
				break;
			case SMX_OP_LOAD_S_ALT:
				alt = new ILLoad( GetFrameVar( params[0] ) );
				break;
			case SMX_OP_LOAD_S_BOTH:
				pri = new ILLoad( GetFrameVar( params[0] ) );
				alt = new ILLoad( GetFrameVar( params[1] ) );
				break;
			case SMX_OP_LOAD_I:
			{
				ILVar* var = GetVar( pri );
				assert( var );
				pri = new ILLoad( var );
				break;
			}

			case SMX_OP_STOR_PRI:
				ilbb.Add( new ILStore( new ILGlobalVar( params[0] ), pri ) );
				break;
			case SMX_OP_STOR_ALT:
				ilbb.Add( new ILStore( new ILGlobalVar( params[0] ), alt ) );
				break;
			case SMX_OP_STOR_S_PRI:
				ilbb.Add( new ILStore( GetFrameVar( params[0] ), pri ) );
				break;
			case SMX_OP_STOR_S_ALT:
				ilbb.Add( new ILStore( GetFrameVar( params[0] ), alt ) );
				break;
			case SMX_OP_STOR_I:
			{
				ILVar* var = GetVar( alt );
				assert( var );
				ilbb.Add( new ILStore( var, pri ) );
				break;
			}

			case SMX_OP_LREF_S_PRI:
				pri = new ILLoad( GetFrameVar( params[0] ) );
				break;
			case SMX_OP_LREF_S_ALT:
				alt = new ILLoad( GetFrameVar( params[0] ) );
				break;
			case SMX_OP_SREF_S_PRI:
				ilbb.Add( new ILStore( GetFrameVar( params[0] ), pri ) );
				break;
			case SMX_OP_SREF_S_ALT:
				ilbb.Add( new ILStore( GetFrameVar( params[0] ), alt ) );
				break;
			
			case SMX_OP_LODB_I:
			{
				ILVar* var = GetVar( pri );
				assert( var );
				pri = new ILLoad( var, params[0] );
				break;
			}
			case SMX_OP_STRB_I:
			{
				ILVar* var = GetVar( alt );
				assert( var );
				ilbb.Add( new ILStore( var, pri, params[0] ) );
				break;
			}

			case SMX_OP_LIDX:
			{
				ILVar* arr = GetVar( alt );
				auto* elem = new ILArrayElementVar( arr, pri );
				pri = new ILLoad( elem );
				break;
			}
			case SMX_OP_IDXADDR:
			{
				ILVar* arr = GetVar( alt );
				assert( arr );
				pri = new ILArrayElementVar( arr, pri );
				break;
			}

			case SMX_OP_ADDR_PRI:
				pri = GetFrameVar( params[0] );
				break;
			case SMX_OP_ADDR_ALT:
				alt = GetFrameVar( params[0] );
				break;

			case SMX_OP_ZERO_PRI:
				pri = new ILConst( 0 );
				break;
			case SMX_OP_ZERO_ALT:
				alt = new ILConst( 0 );
				break;
			case SMX_OP_ZERO:
				ilbb.Add( new ILStore( new ILGlobalVar( params[0] ), new ILConst( 0 ) ) );
				break;
			case SMX_OP_ZERO_S:
				ilbb.Add( new ILStore( GetFrameVar( params[0] ), new ILConst( 0 ) ) );
				break;

			case SMX_OP_MOVE_PRI:
				pri = alt;
				break;
			case SMX_OP_MOVE_ALT:
				alt = pri;
				break;
			case SMX_OP_XCHG:
				std::swap( pri, alt );
				break;
			case SMX_OP_SWAP_PRI:
			{
				ILLocalVar* top = Pop();
				ilbb.Add( Push( pri ) );
				pri = top->value();
				break;
			}
			case SMX_OP_SWAP_ALT:
			{
				ILLocalVar* top = Pop();
				ilbb.Add( Push( alt ) );
				alt = top->value();
				break;
			}

			case SMX_OP_INC_PRI:
				pri = new ILUnary( pri, ILUnary::INC );
				break;
			case SMX_OP_INC_ALT:
				alt = new ILUnary( alt, ILUnary::INC );
				break;
			case SMX_OP_INC:
			{
				auto* var = new ILGlobalVar( params[0] );
				ilbb.Add( new ILStore( var, new ILUnary( new ILLoad( var ), ILUnary::INC ) ) );
				break;
			}
			case SMX_OP_INC_S:
			{
				auto* var = GetFrameVar( params[0] );
				ilbb.Add( new ILStore( var, new ILUnary( new ILLoad( var ), ILUnary::INC ) ) );
				break;
			}
			case SMX_OP_INC_I:
			{
				auto* var = GetVar( pri );
				assert( var );
				ilbb.Add( new ILStore( var, new ILUnary( new ILLoad( var ), ILUnary::INC ) ) );
				break;
			}
			case SMX_OP_DEC_PRI:
				pri = new ILUnary( pri, ILUnary::DEC );
				break;
			case SMX_OP_DEC_ALT:
				alt = new ILUnary( alt, ILUnary::DEC );
				break;
			case SMX_OP_DEC:
			{
				auto* var = new ILGlobalVar( params[0] );
				ilbb.Add( new ILStore( var, new ILUnary( new ILLoad( var ), ILUnary::DEC ) ) );
				break;
			}
			case SMX_OP_DEC_S:
			{
				ILLocalVar* var = GetFrameVar( params[0] );
				ilbb.Add( new ILStore( var, new ILUnary( new ILLoad( var ), ILUnary::DEC ) ) );
				break;
			}
			case SMX_OP_DEC_I:
			{
				auto* var = dynamic_cast<ILGlobalVar*>( pri );
				assert( var );
				ilbb.Add( new ILStore( var, new ILUnary( new ILLoad( var ), ILUnary::DEC ) ) );
				break;
			}
			case SMX_OP_SHL:
				pri = new ILBinary( pri, ILBinary::SHL, alt );
				break;
			case SMX_OP_SHR:
				pri = new ILBinary( pri, ILBinary::SHR, alt );
				break;
			case SMX_OP_SSHR:
				pri = new ILBinary( pri, ILBinary::SSHR, alt );
				break;
			case SMX_OP_SHL_C_PRI:
				pri = new ILBinary( pri, ILBinary::SHL, new ILConst( params[0] ) );
				break;
			case SMX_OP_SHL_C_ALT:
				alt = new ILBinary( alt, ILBinary::SHL, new ILConst( params[0] ) );
				break;
			case SMX_OP_SMUL:
				pri = new ILBinary( pri, ILBinary::MUL, alt );
				break;
			case SMX_OP_SMUL_C:
				pri = new ILBinary( pri, ILBinary::MUL, new ILConst( params[0] ) );
				break;
			case SMX_OP_SDIV:
			{
				ILNode* dividend = pri;
				ILNode* divisor = alt;
				pri = new ILBinary( dividend, ILBinary::DIV, divisor );
				alt = new ILBinary( dividend, ILBinary::MOD, divisor );
				break;
			}
			case SMX_OP_SDIV_ALT:
			{
				ILNode* dividend = alt;
				ILNode* divisor = pri;
				pri = new ILBinary( dividend, ILBinary::DIV, divisor );
				alt = new ILBinary( dividend, ILBinary::MOD, divisor );
				break;
			}
			case SMX_OP_ADD:
			{
				// Indexing into 2D array will generate add
				if( auto* var = dynamic_cast<ILVar*>(alt) )
				{
					pri = new ILArrayElementVar( var, pri );
				}
				else if( auto* var = dynamic_cast<ILVar*>(pri) )
				{
					pri = new ILArrayElementVar( var, alt );
				}
				else
				{
					pri = new ILBinary( pri, ILBinary::ADD, alt );
				}
				break;
			}
			case SMX_OP_ADD_C:
			{
				// add.c is also used to offset into arrays/enum-structs
				if( auto* var = dynamic_cast<ILVar*>(pri) )
				{
					pri = new ILArrayElementVar( var, new ILConst( params[0] ) );
				}
				else
				{
					pri = new ILBinary( pri, ILBinary::ADD, new ILConst( params[0] ) );
				}
				break;
			}
			case SMX_OP_SUB:
				pri = new ILBinary( pri, ILBinary::SUB, alt );
				break;
			case SMX_OP_SUB_ALT:
				pri = new ILBinary( alt, ILBinary::SUB, pri );
				break;
			case SMX_OP_AND:
				pri = new ILBinary( pri, ILBinary::BITAND, alt );
				break;
			case SMX_OP_OR:
				pri = new ILBinary( pri, ILBinary::BITOR, alt );
				break;
			case SMX_OP_XOR:
				pri = new ILBinary( pri, ILBinary::BITOR, alt );
				break;
			case SMX_OP_NOT:
				pri = new ILUnary( pri, ILUnary::NOT );
				break;
			case SMX_OP_NEG:
				pri = new ILUnary( pri, ILUnary::NEG );
				break;
			case SMX_OP_INVERT:
				pri = new ILUnary( pri, ILUnary::INVERT );
				break;

			case SMX_OP_EQ:
				pri = new ILBinary( pri, ILBinary::EQ, alt );
				break;
			case SMX_OP_NEQ:
				pri = new ILBinary( pri, ILBinary::NEQ, alt );
				break;
			case SMX_OP_SLESS:
				pri = new ILBinary( pri, ILBinary::SLESS, alt );
				break;
			case SMX_OP_SLEQ:
				pri = new ILBinary( pri, ILBinary::SLEQ, alt );
				break;
			case SMX_OP_SGRTR:
				pri = new ILBinary( pri, ILBinary::SGRTR, alt );
				break;
			case SMX_OP_SGEQ:
				pri = new ILBinary( pri, ILBinary::SGEQ, alt );
				break;

			case SMX_OP_EQ_C_PRI:
				pri = new ILBinary( pri, ILBinary::EQ, new ILConst( params[0] ) );
				break;
			case SMX_OP_EQ_C_ALT:
				pri = new ILBinary( alt, ILBinary::EQ, new ILConst( params[0] ) );
				break;

			case SMX_OP_FABS:
				pri = new ILUnary( Pop(), ILUnary::FABS );
				break;
			case SMX_OP_FLOAT:
				pri = new ILUnary( Pop(), ILUnary::FLOAT );
				break;
			case SMX_OP_FLOATADD:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri = new ILBinary( left, ILBinary::FLOATADD, right );
				break;
			}
			case SMX_OP_FLOATSUB:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri = new ILBinary( left, ILBinary::FLOATSUB, right );
				break;
			}
			case SMX_OP_FLOATMUL:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri = new ILBinary( left, ILBinary::FLOATMUL, right );
				break;
			}
			case SMX_OP_FLOATDIV:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri = new ILBinary( left, ILBinary::FLOATDIV, right );
				break;
			}
			case SMX_OP_RND_TO_NEAREST:
				pri = new ILUnary( Pop(), ILUnary::RND_TO_NEAREST );
				break;
			case SMX_OP_RND_TO_FLOOR:
				pri = new ILUnary( Pop(), ILUnary::RND_TO_FLOOR );
				break;
			case SMX_OP_RND_TO_CEIL:
				pri = new ILUnary( Pop(), ILUnary::RND_TO_CEIL );
				break;
			case SMX_OP_RND_TO_ZERO:
				pri = new ILUnary( Pop(), ILUnary::RND_TO_ZERO );
				break;

			case SMX_OP_FLOATCMP:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri = new ILBinary( left, ILBinary::FLOATCMP, right );
				break;
			}
			case SMX_OP_FLOAT_GT:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri = new ILBinary( left, ILBinary::FLOATGT, right );
				break;
			}
			case SMX_OP_FLOAT_GE:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri = new ILBinary( left, ILBinary::FLOATGE, right );
				break;
			}
			case SMX_OP_FLOAT_LE:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri = new ILBinary( left, ILBinary::FLOATLE, right );
				break;
			}
			case SMX_OP_FLOAT_LT:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri = new ILBinary( left, ILBinary::FLOATLT, right );
				break;
			}
			case SMX_OP_FLOAT_EQ:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri = new ILBinary( left, ILBinary::FLOATEQ, right );
				break;
			}
			case SMX_OP_FLOAT_NE:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri = new ILBinary( left, ILBinary::FLOATNE, right );
				break;
			}
			case SMX_OP_FLOAT_NOT:
				pri = new ILUnary( Pop(), ILUnary::FLOATNOT );
				break;


			case SMX_OP_CALL:
			{
				auto* nargs = dynamic_cast<ILConst*>(PopValue());
				assert( nargs );
				auto* call = new ILCall( params[0] );
				for( cell_t i = 0; i < nargs->value(); i++ )
				{
					call->AddArg( PopValue() );
				}
				ILTempVar* result = MakeTemp( call );
				ilbb.Add( result );
				pri = result;
				break;
			}
			case SMX_OP_SYSREQ_C:
			{
				ILTempVar* result = MakeTemp( new ILNative( params[0] ) );
				ilbb.Add( result );
				pri = result;
			}
			case SMX_OP_SYSREQ_N:
			{
				cell_t native_index = params[0];
				cell_t nargs = params[1];
				auto* ntv = new ILNative( native_index );
				for( cell_t i = 0; i < nargs; i++ )
				{
					ntv->AddArg( PopValue() );
				}
				ILTempVar* result = MakeTemp( ntv );
				ilbb.Add( result );
				pri = result;
				break;
			}

			case SMX_OP_JUMP:
				ilbb.Add( new ILJump( ilcfg_->FindBlockAt( params[0] ) ) );
				break;
			case SMX_OP_JZER:
				handle_jmp( new ILBinary( pri, ILBinary::EQ, new ILConst( 0 ) ) );
				break;
			case SMX_OP_JNZ:
				handle_jmp( new ILBinary( pri, ILBinary::NEQ, new ILConst( 0 ) ) );
				break;
			case SMX_OP_JEQ:
				handle_jmp( new ILBinary( pri, ILBinary::EQ, alt ) );
				break;
			case SMX_OP_JNEQ:
				handle_jmp( new ILBinary( pri, ILBinary::NEQ, alt ) );
				break;
			case SMX_OP_JSLESS:
				handle_jmp( new ILBinary( pri, ILBinary::SLESS, alt ) );
				break;
			case SMX_OP_JSLEQ:
				handle_jmp( new ILBinary( pri, ILBinary::SLEQ, alt ) );
				break;
			case SMX_OP_JSGRTR:
				handle_jmp( new ILBinary( pri, ILBinary::SGRTR, alt ) );
				break;
			case SMX_OP_JSGEQ:
				handle_jmp( new ILBinary( pri, ILBinary::SGEQ, alt ) );
				break;


			case SMX_OP_SWITCH:
			{
				cell_t* casetbl = smx_->code( params[0] );
				assert( casetbl[0] == SMX_OP_CASETBL );
				cell_t num_cases = casetbl[1];
				ILBlock* default_case = ilcfg_->FindBlockAt( casetbl[2] );
				std::vector<CaseTableEntry> cases;
				cases.reserve( num_cases );
				for( cell_t i = 0; i < num_cases; i++ )
				{
					CaseTableEntry entry;
					entry.value = casetbl[3 + i * 2];
					entry.address = ilcfg_->FindBlockAt( casetbl[3 + i * 2 + 1] );
					cases.push_back( entry );
				}
				ilbb.Add( new ILSwitch( pri, default_case, std::move( cases ) ) );
				break;
			}

			case SMX_OP_RETN:
			{
				ilbb.Add( new ILReturn( pri ) );
				break;
			}

			case SMX_OP_MOVS:
				break;

			case SMX_OP_NONE:
			case SMX_OP_BREAK:
			case SMX_OP_BOUNDS:
				break;

			default:
				assert( 0 && "Unhandled opcode" && op );
				break;
		}


		instr = next_instr;
	}
}

void PcodeLifter::CleanCalls( ILBlock& ilbb )
{
	for( int i = (int)ilbb.num_nodes() - 1; i >= 0; i-- )
	{
		if( auto* var = dynamic_cast<ILTempVar*>(ilbb.node( i )) )
		{
			ILCallable* call = dynamic_cast<ILCallable*>(var->value());
			if( !call )
			{
				continue;
			}

			if( var->num_uses() == 0 )
			{
				// If call result is not used anywhere, remove the temp var
				// Can't remove call entirely since it may have side effects
				ilbb.Replace( i, call );
			}
			else if( var->num_uses() == 1 )
			{
				// Just use call node directly at use site rather than going through temp var
				var->ReplaceUsesWith( call );
				ilbb.Remove( i );
			}
		}
	}
}

void PcodeLifter::PruneVarsInBlock( ILBlock& ilbb )
{
	for( int i = (int)ilbb.num_nodes() - 1; i >= 0; i-- )
	{
		if( auto* var = dynamic_cast<ILVar*>(ilbb.node( i )) )
		{
			if( var->num_uses() == 0 )
			{
				ilbb.Remove( i );
			}
		}
	}
}

void PcodeLifter::MovePhis( ILBlock& ilbb )
{
	// Turn phis into stores on incoming edges
	for( int i = (int)ilbb.num_nodes() - 1; i >= 0; i-- )
	{
		if( auto* tmp = dynamic_cast<ILTempVar*>(ilbb.node( i )) )
		{
			if( auto* phi = dynamic_cast<ILPhi*>(tmp->value()) )
			{
				// Add declaration at immed_dominator
				tmp->SetValue( nullptr );
				ilbb.immed_dominator()->AddToEnd( tmp );

				// Add stores on incoming edges
				assert( phi->num_inputs() == ilbb.num_in_edges() );
				for( size_t inp = 0; inp < phi->num_inputs(); inp++ )
				{
					ILBlock& in = ilbb.in_edge( inp );
					in.AddToEnd( new ILStore( tmp, phi->input( inp ) ) );
				}

				// Remove from current block
				ilbb.Remove( i );
			}
		}
	}
}

void PcodeLifter::CompoundConditions() const
{
	bool changed = true;
	while( changed )
	{
		changed = false;

		for( size_t i = 0; i < ilcfg_->num_blocks(); i++ )
		{
			ILBlock& bb = ilcfg_->block( i );
			if( bb.num_out_edges() != 2 || dynamic_cast<ILSwitch*>(bb.Last()) )
				continue;

			ILBlock& then_branch = bb.out_edge( 0 );
			ILBlock& else_branch = bb.out_edge( 1 );

			// X || Y
			if( else_branch.num_out_edges() == 2 &&
				else_branch.num_nodes() == 1 &&
				else_branch.num_in_edges() == 1 &&
				&else_branch.out_edge( 0 ) == &then_branch )
			{
				CompoundXandY( bb, else_branch, then_branch, else_branch.out_edge( 1 ) );
				changed = true;
			}

			// X && Y
			if( then_branch.num_out_edges() == 2 &&
				then_branch.num_nodes() == 1 &&
				then_branch.num_in_edges() == 1 &&
				&then_branch.out_edge( 1 ) == &else_branch )
			{
				CompoundXorY( bb, then_branch, then_branch.out_edge( 0 ), else_branch );
				changed = true;
			}

			// 2 other cases also exist
			// !X || Y
			// !X && Y
			// but these shouldn't be emitted by compiler
		}
	}
}

void PcodeLifter::CompoundXandY( ILBlock& x, ILBlock& y, ILBlock& then_branch, ILBlock& else_branch ) const
{
	auto* x_cond = dynamic_cast<ILJumpCond*>(x.Last());
	auto* y_cond = dynamic_cast<ILJumpCond*>(y.Last());
	assert( x_cond && y_cond );

	x_cond->Invert();
	y_cond->Invert();

	auto* new_cond = new ILJumpCond(
		new ILBinary( x_cond->condition(), ILBinary::AND, y_cond->condition() ),
		&then_branch,
		&else_branch );

	x.Replace( x.num_nodes() - 1, new_cond );

	x.ReplaceOutEdge( y, else_branch );
	else_branch.ReplaceInEdge( y, x );
	then_branch.RemoveInEdge( y );
	ilcfg_->Remove( y );
}

void PcodeLifter::CompoundXorY( ILBlock& x, ILBlock& y, ILBlock& then_branch, ILBlock& else_branch ) const
{
	auto* x_cond = dynamic_cast<ILJumpCond*>(x.Last());
	auto* y_cond = dynamic_cast<ILJumpCond*>(y.Last());
	assert( x_cond && y_cond );

	auto* new_cond = new ILJumpCond(
		new ILBinary( x_cond->condition(), ILBinary::AND, y_cond->condition() ),
		&then_branch,
		&else_branch );

	x.Replace( x.num_nodes() - 1, new_cond );

	x.ReplaceOutEdge( y, then_branch );
	then_branch.ReplaceInEdge( y, x );
	else_branch.RemoveInEdge( y );
	ilcfg_->Remove( y );
}

ILLocalVar* PcodeLifter::Push( ILNode* value )
{
	int offset = (ilcfg_->nargs() + 3) - (int)expr_stack_->stack.size() - 1;
	expr_stack_->stack.push_back( new ILLocalVar( offset * 4, value ) );
	return expr_stack_->stack.back();
}

ILLocalVar* PcodeLifter::Pop()
{
	if( expr_stack_->stack.empty() )
	{
		assert( 0 );
		return nullptr;
	}

	ILLocalVar* top = expr_stack_->stack.back();
	expr_stack_->stack.pop_back();
	return top;
}

ILNode* PcodeLifter::PopValue()
{
	ILLocalVar* var = Pop();
	if( !var )
		return nullptr;

	ILNode* val = var->value();
	var->ReplaceParam( val, nullptr );
	return val;
}

ILLocalVar* PcodeLifter::GetFrameVar( int offset )
{
	assert( expr_stack_ );
	
	return expr_stack_->stack[(ilcfg_->nargs() + 3) - 1 - offset/4];
}

ILNode* PcodeLifter::GetFrameVal( int offset )
{
	return GetFrameVar( offset )->value();
}

void PcodeLifter::SetFrameVal( int offset, ILNode* val )
{
	GetFrameVar( offset )->SetValue( val );
}

ILTempVar* PcodeLifter::MakeTemp( ILNode* value )
{
	return new ILTempVar( num_temps_++, value );
}

ILVar* PcodeLifter::GetVar( ILNode* node ) const
{
	// Sometimes a global address gets loaded by its constant address
	// We don't know if it's a constant or a global address until it
	// actually gets used, so make that adjustment here
	if( auto* constant = dynamic_cast<ILConst*>(node) )
	{
		node = new ILGlobalVar( constant->value() );
		constant->ReplaceUsesWith( node );
	}

	// Turn addition into indexing operation
	if( auto* add = dynamic_cast<ILBinary*>( node ) )
	{
		if( add->op() == ILBinary::ADD )
		{
			node = new ILArrayElementVar( add->left(), add->right() );
			add->ReplaceUsesWith( node );
		}
	}

	// Remove load wrapped around arg
	if( auto* load = dynamic_cast<ILLoad*>( node ) )
	{
		return load->var();
	}

	return dynamic_cast<ILVar*>(node);
}
