#include "il-disasm.h"

std::string ILDisassembler::DisassembleNode( ILNode* node )
{
	disasm_.str( {} );
	disasm_.clear();
	Visit( node );
	return disasm_.str();
}

std::string ILDisassembler::DisassembleBlock( const ILBlock& block )
{
	std::stringstream block_disasm;
	for( size_t node = 0; node < block.num_nodes(); node++ )
	{
		block_disasm << DisassembleNode( block.node( node ) ) << '\n';
	}
	return block_disasm.str();
}

void ILDisassembler::VisitConst( ILConst* node )
{
	disasm_ << node->value();
}

void ILDisassembler::VisitUnary( ILUnary* node )
{
	switch( node->op() )
	{
		case ILUnary::NOT:
			disasm_ << "!" << Visit( node->val() );
			break;
		case ILUnary::NEG:
			disasm_ << "-" << Visit( node->val() );
			break;
		case ILUnary::INVERT:
			disasm_ << "~" << Visit( node->val() );
			break;

		case ILUnary::FABS:
		case ILUnary::FLOAT:
		case ILUnary::FLOATNOT:
		case ILUnary::RND_TO_NEAREST:
		case ILUnary::RND_TO_CEIL:
		case ILUnary::RND_TO_ZERO:
		case ILUnary::RND_TO_FLOOR:
			disasm_ << "<err>";
			break;

		case ILUnary::INC:
			disasm_ << "++" << Visit( node->val() );
			break;
		case ILUnary::DEC:
			disasm_ << "--" << Visit( node->val() );
			break;
	}
}

void ILDisassembler::VisitBinary( ILBinary* node )
{
	switch( node->op() )
	{
		case ILBinary::ADD:      disasm_ << Visit( node->left() ) << " + " << Visit( node->right() ); break;
		case ILBinary::SUB:      disasm_ << Visit( node->left() ) << " - " << Visit( node->right() ); break;
		case ILBinary::DIV:      disasm_ << Visit( node->left() ) << " / " << Visit( node->right() ); break;
		case ILBinary::MUL:      disasm_ << Visit( node->left() ) << " * " << Visit( node->right() ); break;
		case ILBinary::MOD:      disasm_ << Visit( node->left() ) << " % " << Visit( node->right() ); break;
		case ILBinary::SHL:      disasm_ << Visit( node->left() ) << " << " << Visit( node->right() ); break;
		case ILBinary::SHR:      disasm_ << Visit( node->left() ) << " >> " << Visit( node->right() ); break;
		case ILBinary::SSHR:     disasm_ << Visit( node->left() ) << " >> " << Visit( node->right() ); break;
		case ILBinary::AND:      disasm_ << Visit( node->left() ) << " & " << Visit( node->right() ); break;
		case ILBinary::OR:       disasm_ << Visit( node->left() ) << " | " << Visit( node->right() ); break;
		case ILBinary::XOR:      disasm_ << Visit( node->left() ) << " ^ " << Visit( node->right() ); break;

		case ILBinary::EQ:       disasm_ << Visit( node->left() ) << " == " << Visit( node->right() ); break;
		case ILBinary::NEQ:      disasm_ << Visit( node->left() ) << " != " << Visit( node->right() ); break;
		case ILBinary::SGRTR:    disasm_ << Visit( node->left() ) << " > " << Visit( node->right() ); break;
		case ILBinary::SGEQ:     disasm_ << Visit( node->left() ) << " >= " << Visit( node->right() ); break;
		case ILBinary::SLESS:    disasm_ << Visit( node->left() ) << " < " << Visit( node->right() ); break;
		case ILBinary::SLEQ:     disasm_ << Visit( node->left() ) << " <= " << Visit( node->right() ); break;

		case ILBinary::FLOATADD: disasm_ << Visit( node->left() ) << " f+ " << Visit( node->right() ); break;
		case ILBinary::FLOATSUB: disasm_ << Visit( node->left() ) << " f- " << Visit( node->right() ); break;
		case ILBinary::FLOATMUL: disasm_ << Visit( node->left() ) << " f* " << Visit( node->right() ); break;
		case ILBinary::FLOATDIV: disasm_ << Visit( node->left() ) << " f/ " << Visit( node->right() ); break;

		case ILBinary::FLOATCMP: disasm_ << Visit( node->left() ) << " fcmp " << Visit( node->right() ); break;
		case ILBinary::FLOATGT:  disasm_ << Visit( node->left() ) << " f> " << Visit( node->right() ); break;
		case ILBinary::FLOATGE:  disasm_ << Visit( node->left() ) << " f>= " << Visit( node->right() ); break;
		case ILBinary::FLOATLE:  disasm_ << Visit( node->left() ) << " f<= " << Visit( node->right() ); break;
		case ILBinary::FLOATLT:  disasm_ << Visit( node->left() ) << " f< " << Visit( node->right() ); break;
		case ILBinary::FLOATEQ:  disasm_ << Visit( node->left() ) << " f== " << Visit( node->right() ); break;
		case ILBinary::FLOATNE:  disasm_ << Visit( node->left() ) << " f!= " << Visit( node->right() ); break;
	}
}

void ILDisassembler::VisitLocalVar( ILLocalVar* node )
{
	disasm_ << "local_" << node->stack_offset();
}

void ILDisassembler::VisitGlobalVar( ILGlobalVar* node )
{
	disasm_ << "global_" << node->addr();
}

void ILDisassembler::VisitHeapVar( ILHeapVar* node )
{
	disasm_ << "heap_" << node->size();
}

void ILDisassembler::VisitArrayElementVar( ILArrayElementVar* node )
{
	disasm_ << Visit( node->base() ) << "[" << Visit( node->index() ) << "]";
}

void ILDisassembler::VisitLoad( ILLoad* node )
{
	disasm_ << Visit( node->var() );
}

void ILDisassembler::VisitStore( ILStore* node )
{
	disasm_ << Visit( node->var() ) << " = " << Visit( node->val() );
}

void ILDisassembler::VisitJump( ILJump* node )
{
	disasm_ << "goto BB" << node->target()->id();
}

void ILDisassembler::VisitJumpCond( ILJumpCond* node )
{
	disasm_ << "if " << Visit( node->condition() ) << " goto BB" << node->true_branch()->id() << " else BB" << node->false_branch()->id();
}

void ILDisassembler::VisitCall( ILCall* node )
{
	disasm_ << "func_" << node->addr() << "(";
	for( size_t i = 0; i < node->num_args(); i++ )
	{
		disasm_ << Visit( node->arg( i ) );
		if( i != node->num_args() - 1 )
		{
			disasm_ << ", ";
		}
	}
	disasm_ << ")";
}

void ILDisassembler::VisitNative( ILNative* node )
{
	disasm_ << "native_" << node->native_index() << "(";
	for( size_t i = 0; i < node->num_args(); i++ )
	{
		disasm_ << Visit( node->arg( i ) );
		if( i != node->num_args() - 1 )
		{
			disasm_ << ", ";
		}
	}
	disasm_ << ")";
}

void ILDisassembler::VisitRet( ILRet* node )
{
	disasm_ << "ret";
}

std::string ILDisassembler::Visit( ILNode* node )
{
	node->Accept( this );
	return "";
}
