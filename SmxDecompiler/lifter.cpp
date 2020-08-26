#include "lifter.h"

#include "il.h"
#include "smx-opcodes.h"
#include <cassert>

ILControlFlowGraph PcodeLifter::Lift( const ControlFlowGraph& cfg )
{
	pri_ = nullptr;
	alt_ = nullptr;

	ilcfg_.SetNumArgs( cfg.nargs() );

	for( size_t i = 0; i < cfg.num_blocks(); i++ )
	{
		BasicBlock& bb = cfg.block( i );
		ilcfg_.AddBlock( bb.id(), (cell_t)((intptr_t)bb.start() - (intptr_t)smx_->code()) );
	}

	block_stacks_.resize( ilcfg_.num_blocks() );
	for( size_t i = 0; i < cfg.num_blocks(); i++ )
	{
		BasicBlock& bb = cfg.block( i );
		ILBlock& ilbb = ilcfg_.block( i );
		LiftBlock( bb, ilbb );
	}

	return std::move( ilcfg_ );
}

void PcodeLifter::LiftBlock( BasicBlock& bb, ILBlock& ilbb )
{
	stack_ = &block_stacks_[ilbb.id()];
	for( size_t i = 0; i < ilbb.num_in_edges(); i++ )
	{
		ILBlock* pred = ilbb.in_edge( i );
		
		// No back edges
		if( pred->id() >= ilbb.id() )
		{
			continue;
		}

		if( stack_->empty() )
		{
			*stack_ = block_stacks_[pred->id()];
		}
	}

	for( size_t i = 0; i < bb.num_out_edges(); i++ )
	{
		BasicBlock* out = bb.out_edge( i );
		ILBlock& ilout = ilcfg_.block( out->id() );
		ilbb.AddTarget( &ilout );
	}

	const cell_t* instr = bb.start();
	while( instr < bb.end() )
	{
		auto op = (SmxOpcode)instr[0];
		const cell_t* params = instr + 1;
		const auto& info = SmxInstrInfo::Get( op );
		const cell_t* next_instr = instr + info.num_params + 1;

		auto handle_jmp = [&]( ILBinary* cmp ) {
			ILBlock* true_branch = ilcfg_.FindBlockAt( params[0] );
			ILBlock* false_branch = ilcfg_.FindBlockAt( (cell_t)((intptr_t)next_instr - (intptr_t)smx_->code()) );
			assert( true_branch && false_branch );
			ilbb.Add( new ILJumpCond( cmp, true_branch, false_branch ) );
		};

		switch( op )
		{
			case SMX_OP_PROC:
			{
				for( int i = 0; i < ilcfg_.nargs(); i++ )
				{
					Push( new ILLocalVar( 12 + i * 4 ) );
				}

				Push( new ILConst( ilcfg_.nargs() ) ); // Number of args
				Push( new ILLocalVar( 4 ) ); // Frame pointer
				Push( new ILLocalVar( 0 ) ); // Heap pointer

				break;
			}

			case SMX_OP_STACK:
			{
				cell_t amount = params[0];
				if( amount < 0 )
				{
					for( cell_t i = 0; i < -amount / 4; i++ )
					{
						auto* var = new ILLocalVar( i * 4 );
						Push( var );
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
				alt_ = new ILHeapVar( params[0] );
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
					Push( new ILLoad( new ILGlobalVar( params[i] ) ) );
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
					Push( new ILLoad( new ILLocalVar( params[i] ) ) );
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
					Push( new ILConst( params[i] ) );
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
					Push( new ILLocalVar( params[i] ) );
				}

				break;
			}

			case SMX_OP_PUSH_PRI:
				Push( pri_ );
				break;
			case SMX_OP_PUSH_ALT:
				Push( alt_ );
				break;

			case SMX_OP_POP_PRI:
				pri_ = Pop();
				break;
			case SMX_OP_POP_ALT:
				alt_ = Pop();
				break;

			case SMX_OP_CONST_PRI:
				pri_ = new ILConst( params[0] );
				break;
			case SMX_OP_CONST_ALT:
				alt_ = new ILConst( params[0] );
				break;
			case SMX_OP_CONST:
				ilbb.Add( new ILStore( new ILGlobalVar( params[0] ), new ILConst( params[1] ) ) );
				break;
			case SMX_OP_CONST_S:
				ilbb.Add( new ILStore( new ILLocalVar( params[0] ), new ILConst( params[1] ) ) );
				break;

			case SMX_OP_LOAD_PRI:
				pri_ = new ILLoad( new ILGlobalVar( params[0] ) );
				break;
			case SMX_OP_LOAD_ALT:
				alt_ = new ILLoad( new ILGlobalVar( params[0] ) );
				break;
			case SMX_OP_LOAD_BOTH:
				pri_ = new ILLoad( new ILGlobalVar( params[0] ) );
				alt_ = new ILLoad( new ILGlobalVar( params[1] ) );
				break;
			case SMX_OP_LOAD_S_PRI:
				pri_ = new ILLoad( new ILLocalVar( params[0] ) );
				break;
			case SMX_OP_LOAD_S_ALT:
				alt_ = new ILLoad( new ILLocalVar( params[0] ) );
				break;
			case SMX_OP_LOAD_S_BOTH:
				pri_ = new ILLoad( new ILLocalVar( params[0] ) );
				alt_ = new ILLoad( new ILLocalVar( params[1] ) );
				break;
			case SMX_OP_LOAD_I:
			{
				ILVar* var = dynamic_cast<ILVar*>( pri_ );
				assert( var );
				pri_ = new ILLoad( var );
				break;
			}

			case SMX_OP_STOR_PRI:
				ilbb.Add( new ILStore( new ILGlobalVar( params[0] ), pri_ ) );
				break;
			case SMX_OP_STOR_ALT:
				ilbb.Add( new ILStore( new ILGlobalVar( params[0] ), alt_ ) );
				break;
			case SMX_OP_STOR_S_PRI:
				ilbb.Add( new ILStore( new ILLocalVar( params[0] ), pri_ ) );
				break;
			case SMX_OP_STOR_S_ALT:
				ilbb.Add( new ILStore( new ILLocalVar( params[0] ), alt_ ) );
				break;
			case SMX_OP_STOR_I:
			{
				ILVar* var = dynamic_cast<ILVar*>( alt_ );
				assert( var );
				ilbb.Add( new ILStore( var, pri_ ) );
				break;
			}

			case SMX_OP_LREF_S_PRI:
				assert( 0 );
				break;
			case SMX_OP_LREF_S_ALT:
				assert( 0 );
				break;
			case SMX_OP_SREF_S_PRI:
				assert( 0 );
				break;
			case SMX_OP_SREF_S_ALT:
				assert( 0 );
				break;
			
			case SMX_OP_LODB_I:
			{
				ILVar* var = dynamic_cast<ILVar*>( pri_ );
				assert( var );
				pri_ = new ILLoad( var, params[0] );
				break;
			}
			case SMX_OP_STRB_I:
			{
				ILVar* var = dynamic_cast<ILVar*>( alt_ );
				assert( var );
				ilbb.Add( new ILStore( var, pri_, params[0] ) );
				break;
			}

			case SMX_OP_LIDX:
			{
				ILVar* arr = dynamic_cast<ILVar*>( alt_ );
				assert( arr );
				auto* elem = new ILArrayElementVar( arr, pri_ );
				ilbb.Add( new ILLoad( elem ) );
				break;
			}
			case SMX_OP_IDXADDR:
			{
				ILVar* arr = dynamic_cast<ILVar*>( alt_ );
				assert( arr );
				pri_ = new ILArrayElementVar( arr, pri_ );
				break;
			}

			case SMX_OP_ADDR_PRI:
				pri_ = new ILLocalVar( params[0] );
				break;
			case SMX_OP_ADDR_ALT:
				alt_ = new ILLocalVar( params[0] );
				break;

			case SMX_OP_ZERO_PRI:
				pri_ = new ILConst( 0 );
				break;
			case SMX_OP_ZERO_ALT:
				alt_ = new ILConst( 0 );
				break;
			case SMX_OP_ZERO:
				ilbb.Add( new ILStore( new ILGlobalVar( params[0] ), new ILConst( 0 ) ) );
				break;
			case SMX_OP_ZERO_S:
				ilbb.Add( new ILStore( new ILLocalVar( params[0] ), new ILConst( 0 ) ) );
				break;

			case SMX_OP_MOVE_PRI:
				pri_ = alt_;
				break;
			case SMX_OP_MOVE_ALT:
				alt_ = pri_;
				break;
			case SMX_OP_XCHG:
				std::swap( pri_, alt_ );
				break;
			case SMX_OP_SWAP_PRI:
				std::swap( pri_, stack_->back() );
				break;
			case SMX_OP_SWAP_ALT:
				std::swap( alt_, stack_->back() );
				break;

			case SMX_OP_INC_PRI:
				pri_ = new ILUnary( pri_, ILUnary::INC );
				break;
			case SMX_OP_INC_ALT:
				alt_ = new ILUnary( alt_, ILUnary::INC );
				break;
			case SMX_OP_INC:
			{
				auto* var = new ILGlobalVar( params[0] );
				ilbb.Add( new ILStore( var, new ILUnary( new ILLoad( var ), ILUnary::INC ) ) );
				break;
			}
			case SMX_OP_INC_S:
			{
				auto* var = new ILLocalVar( params[0] );
				ilbb.Add( new ILStore( var, new ILUnary( new ILLoad( var ), ILUnary::INC ) ) );
				break;
			}
			case SMX_OP_INC_I:
			{
				auto* var = dynamic_cast<ILGlobalVar*>( pri_ );
				assert( var );
				ilbb.Add( new ILStore( var, new ILUnary( new ILLoad( var ), ILUnary::INC ) ) );
				break;
			}
			case SMX_OP_DEC_PRI:
				pri_ = new ILUnary( pri_, ILUnary::DEC );
				break;
			case SMX_OP_DEC_ALT:
				alt_ = new ILUnary( alt_, ILUnary::DEC );
				break;
			case SMX_OP_DEC:
			{
				auto* var = new ILGlobalVar( params[0] );
				ilbb.Add( new ILStore( var, new ILUnary( new ILLoad( var ), ILUnary::DEC ) ) );
				break;
			}
			case SMX_OP_DEC_S:
			{
				auto* var = new ILLocalVar( params[0] );
				ilbb.Add( new ILStore( var, new ILUnary( new ILLoad( var ), ILUnary::DEC ) ) );
				break;
			}
			case SMX_OP_DEC_I:
			{
				auto* var = dynamic_cast<ILGlobalVar*>( pri_ );
				assert( var );
				ilbb.Add( new ILStore( var, new ILUnary( new ILLoad( var ), ILUnary::DEC ) ) );
				break;
			}
			case SMX_OP_SHL:
				pri_ = new ILBinary( pri_, ILBinary::SHL, alt_ );
				break;
			case SMX_OP_SHR:
				pri_ = new ILBinary( pri_, ILBinary::SHR, alt_ );
				break;
			case SMX_OP_SSHR:
				pri_ = new ILBinary( pri_, ILBinary::SSHR, alt_ );
				break;
			case SMX_OP_SHL_C_PRI:
				pri_ = new ILBinary( pri_, ILBinary::SHL, new ILConst( params[0] ) );
				break;
			case SMX_OP_SHL_C_ALT:
				alt_ = new ILBinary( alt_, ILBinary::SHL, new ILConst( params[0] ) );
				break;
			case SMX_OP_SMUL:
				pri_ = new ILBinary( pri_, ILBinary::MUL, alt_ );
				break;
			case SMX_OP_SMUL_C:
				pri_ = new ILBinary( pri_, ILBinary::MUL, new ILConst( params[0] ) );
				break;
			case SMX_OP_SDIV:
			{
				ILNode* dividend = pri_;
				ILNode* divisor = alt_;
				pri_ = new ILBinary( dividend, ILBinary::DIV, divisor );
				alt_ = new ILBinary( dividend, ILBinary::MOD, divisor );
				break;
			}
			case SMX_OP_SDIV_ALT:
			{
				ILNode* dividend = alt_;
				ILNode* divisor = pri_;
				pri_ = new ILBinary( dividend, ILBinary::DIV, divisor );
				alt_ = new ILBinary( dividend, ILBinary::MOD, divisor );
				break;
			}
			case SMX_OP_ADD:
				pri_ = new ILBinary( pri_, ILBinary::ADD, alt_ );
				break;
			case SMX_OP_ADD_C:
				pri_ = new ILBinary( pri_, ILBinary::ADD, new ILConst( params[0] ) );
				break;
			case SMX_OP_SUB:
				pri_ = new ILBinary( pri_, ILBinary::SUB, alt_ );
				break;
			case SMX_OP_SUB_ALT:
				pri_ = new ILBinary( alt_, ILBinary::SUB, pri_ );
				break;
			case SMX_OP_AND:
				pri_ = new ILBinary( pri_, ILBinary::AND, alt_ );
				break;
			case SMX_OP_OR:
				pri_ = new ILBinary( pri_, ILBinary::OR, alt_ );
				break;
			case SMX_OP_XOR:
				pri_ = new ILBinary( pri_, ILBinary::OR, alt_ );
				break;
			case SMX_OP_NOT:
				pri_ = new ILUnary( pri_, ILUnary::NOT );
				break;
			case SMX_OP_NEG:
				pri_ = new ILUnary( pri_, ILUnary::NEG );
				break;
			case SMX_OP_INVERT:
				pri_ = new ILUnary( pri_, ILUnary::INVERT );
				break;

			case SMX_OP_EQ:
				pri_ = new ILBinary( pri_, ILBinary::EQ, alt_ );
				break;
			case SMX_OP_NEQ:
				pri_ = new ILBinary( pri_, ILBinary::NEQ, alt_ );
				break;
			case SMX_OP_SLESS:
				pri_ = new ILBinary( pri_, ILBinary::SLESS, alt_ );
				break;
			case SMX_OP_SLEQ:
				pri_ = new ILBinary( pri_, ILBinary::SLEQ, alt_ );
				break;
			case SMX_OP_SGRTR:
				pri_ = new ILBinary( pri_, ILBinary::SGRTR, alt_ );
				break;
			case SMX_OP_SGEQ:
				pri_ = new ILBinary( pri_, ILBinary::SGEQ, alt_ );
				break;

			case SMX_OP_EQ_C_PRI:
				pri_ = new ILBinary( pri_, ILBinary::EQ, new ILConst( params[0] ) );
				break;
			case SMX_OP_EQ_C_ALT:
				pri_ = new ILBinary( alt_, ILBinary::EQ, new ILConst( params[0] ) );
				break;

			case SMX_OP_FABS:
				pri_ = new ILUnary( Pop(), ILUnary::FABS );
				break;
			case SMX_OP_FLOAT:
				pri_ = new ILUnary( Pop(), ILUnary::FLOAT );
				break;
			case SMX_OP_FLOATADD:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri_ = new ILBinary( left, ILBinary::FLOATADD, right );
				break;
			}
			case SMX_OP_FLOATSUB:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri_ = new ILBinary( left, ILBinary::FLOATSUB, right );
				break;
			}
			case SMX_OP_FLOATMUL:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri_ = new ILBinary( left, ILBinary::FLOATMUL, right );
				break;
			}
			case SMX_OP_FLOATDIV:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri_ = new ILBinary( left, ILBinary::FLOATDIV, right );
				break;
			}
			case SMX_OP_RND_TO_NEAREST:
				pri_ = new ILUnary( Pop(), ILUnary::RND_TO_NEAREST );
				break;
			case SMX_OP_RND_TO_FLOOR:
				pri_ = new ILUnary( Pop(), ILUnary::RND_TO_FLOOR );
				break;
			case SMX_OP_RND_TO_CEIL:
				pri_ = new ILUnary( Pop(), ILUnary::RND_TO_CEIL );
				break;
			case SMX_OP_RND_TO_ZERO:
				pri_ = new ILUnary( Pop(), ILUnary::RND_TO_ZERO );
				break;

			case SMX_OP_FLOATCMP:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri_ = new ILBinary( left, ILBinary::FLOATCMP, right );
				break;
			}
			case SMX_OP_FLOAT_GT:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri_ = new ILBinary( left, ILBinary::FLOATGT, right );
				break;
			}
			case SMX_OP_FLOAT_GE:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri_ = new ILBinary( left, ILBinary::FLOATGE, right );
				break;
			}
			case SMX_OP_FLOAT_LE:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri_ = new ILBinary( left, ILBinary::FLOATLE, right );
				break;
			}
			case SMX_OP_FLOAT_LT:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri_ = new ILBinary( left, ILBinary::FLOATLT, right );
				break;
			}
			case SMX_OP_FLOAT_EQ:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri_ = new ILBinary( left, ILBinary::FLOATEQ, right );
				break;
			}
			case SMX_OP_FLOAT_NE:
			{
				ILNode* left = Pop();
				ILNode* right = Pop();
				pri_ = new ILBinary( left, ILBinary::FLOATNE, right );
				break;
			}
			case SMX_OP_FLOAT_NOT:
				pri_ = new ILUnary( Pop(), ILUnary::FLOATNOT );
				break;


			case SMX_OP_CALL:
			{
				auto* nargs = dynamic_cast<ILConst*>( Pop() );
				assert( nargs );
				auto* call = new ILCall( params[0] );
				for( cell_t i = 0; i < nargs->value(); i++ )
				{
					call->AddArg( Pop() );
				}
				ilbb.Add( call );
				pri_ = call;
				break;
			}
			case SMX_OP_SYSREQ_C:
			{
				pri_ = new ILNative( params[0] );
				ilbb.Add( pri_ );
			}
			case SMX_OP_SYSREQ_N:
			{
				cell_t native_index = params[0];
				cell_t nargs = params[1];
				auto* ntv = new ILNative( native_index );
				for( cell_t i = 0; i < nargs; i++ )
				{
					ntv->AddArg( Pop() );
				}
				ilbb.Add( ntv );
				pri_ = ntv;
				break;
			}

			case SMX_OP_JUMP:
				ilbb.Add( new ILJump( ilcfg_.FindBlockAt( params[0] ) ) );
				break;
			case SMX_OP_JZER:
				handle_jmp( new ILBinary( pri_, ILBinary::EQ, new ILConst( 0 ) ) );
				break;
			case SMX_OP_JNZ:
				handle_jmp( new ILBinary( pri_, ILBinary::NEQ, new ILConst( 0 ) ) );
				break;
			case SMX_OP_JEQ:
				handle_jmp( new ILBinary( pri_, ILBinary::EQ, alt_ ) );
				break;
			case SMX_OP_JNEQ:
				handle_jmp( new ILBinary( pri_, ILBinary::NEQ, alt_ ) );
				break;
			case SMX_OP_JSLESS:
				handle_jmp( new ILBinary( pri_, ILBinary::SLESS, alt_ ) );
				break;
			case SMX_OP_JSLEQ:
				handle_jmp( new ILBinary( pri_, ILBinary::SLEQ, alt_ ) );
				break;
			case SMX_OP_JSGRTR:
				handle_jmp( new ILBinary( pri_, ILBinary::SGRTR, alt_ ) );
				break;
			case SMX_OP_JSGEQ:
				handle_jmp( new ILBinary( pri_, ILBinary::SGEQ, alt_ ) );
				break;

			case SMX_OP_RETN:
			{
				ilbb.Add( new ILRet() );
				break;
			}
			case SMX_OP_BREAK:
				break;

			default:
				assert( 0 );
				break;
		}


		instr = next_instr;
	}
}

void PcodeLifter::Push( ILNode* node )
{
	stack_->push_back( node );
}

ILNode* PcodeLifter::Pop()
{
	if( stack_->empty() )
	{
		assert( 0 );
		return nullptr;
	}

	ILNode* top = stack_->back();
	stack_->pop_back();
	return top;
}

ILNode* PcodeLifter::GetFrameValue( int offset )
{
	assert( stack_ );
	if( offset > 0 )
	{
		// Probably an arg
		return ( *stack_ )[offset / 4];
	}
	else
	{
		return ( *stack_ )[(-offset - (12 + ilcfg_.nargs()*4)) / 4];
	}
}
