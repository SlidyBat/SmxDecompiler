#include "typer.h"

class SmxVariableVisitor : public RecursiveILVisitor
{
public:
	SmxVariableVisitor( SmxFile& smx, const SmxFunction* func ) :
		smx_( &smx ),
		func_( func )
	{}

	virtual void VisitLocalVar( ILLocalVar* node ) override
	{
		RecursiveILVisitor::VisitLocalVar( node );

		// This has already been filled, can skip
		if( node->smx_var() )
			return;

		for( size_t i = 0; i < func_->num_locals; i++ )
		{
			if( func_->locals[i].address == node->stack_offset() )
			{
				node->SetSmxVar( &func_->locals[i] );
				break;
			}
		}

		if( node->smx_var() )
			node->SetType( &node->smx_var()->type );
	}
	virtual void VisitGlobalVar( ILGlobalVar* node ) override
	{
		// This has already been filled, can skip
		if( node->smx_var() )
			return;

		SmxVariable* var = smx_->FindGlobalAt( node->addr() );
		if( !var )
			return;

		node->SetSmxVar( var );
		node->SetType( &var->type );
	}
	virtual void VisitCall( ILCall* node ) override
	{
		RecursiveILVisitor::VisitCall( node );

		SmxFunction* func = smx_->FindFunctionAt( node->addr() );
		if( !func )
			return;

		node->SetType( func->signature.ret );

		for( size_t i = 0; i < std::min( func->signature.nargs, node->num_args() ); i++ )
		{
			ILNode* arg = node->arg( i );
			if( arg->type() )
				continue;
			arg->SetType( &func->signature.args[i].type );
		}
	}
	virtual void VisitNative( ILNative* node ) override
	{
		RecursiveILVisitor::VisitNative( node );

		SmxNative* func = smx_->FindNativeByIndex( node->native_index() );
		if( !func )
			return;

		node->SetType( func->signature.ret );

		for( size_t i = 0; i < std::min(func->signature.nargs, node->num_args()); i++ )
		{
			ILNode* arg = node->arg( i );
			if( arg->type() )
				continue;
			arg->SetType( &func->signature.args[i].type );
		}
	}
private:
	SmxFile* smx_;
	const SmxFunction* func_;
};

class TypePropagator : public ILVisitor
{
public:
	TypePropagator( const SmxFunction* func ) :
		func_( func )
	{
		int_type_ = new SmxVariableType;
		int_type_->tag = SmxVariableType::INT;

		bool_type_ = new SmxVariableType;
		bool_type_->tag = SmxVariableType::BOOL;

		float_type_ = new SmxVariableType;
		float_type_->tag = SmxVariableType::FLOAT;
	}

	void Visit( ILNode* node )
	{
		node->Accept( this );
	}

	virtual void VisitConst( ILConst* node ) override
	{
		if( !node->type() )
			node->SetType( type() );
	}
	virtual void VisitUnary( ILUnary* node ) override
	{
		switch( node->op() )
		{	
		case ILUnary::NOT:
			node->SetType( bool_type_ );
			PushType( type() );
			break;

		case ILUnary::NEG:
		case ILUnary::INVERT:
		case ILUnary::INC:
		case ILUnary::DEC:
			node->SetType( int_type_ );
			PushType( int_type_ );
			break;

		case ILUnary::FABS:
		case ILUnary::FLOAT:
		case ILUnary::FLOATNOT:
		case ILUnary::RND_TO_NEAREST:
		case ILUnary::RND_TO_CEIL:
		case ILUnary::RND_TO_ZERO:
		case ILUnary::RND_TO_FLOOR:
			node->SetType( float_type_ );
			PushType( float_type_ );
			break;
		
		default:
			assert( 0 );
			PushType( type() );
			break;
		}

		Visit( node->val() );

		PopType();
	}
	virtual void VisitBinary( ILBinary* node ) override
	{
		switch( node->op() )
		{
			case ILBinary::ADD:
			case ILBinary::SUB:
			case ILBinary::DIV:
			case ILBinary::MUL:
			case ILBinary::MOD:
			case ILBinary::SHL:
			case ILBinary::SHR:
			case ILBinary::SSHR:
			case ILBinary::BITAND:
			case ILBinary::BITOR:
			case ILBinary::XOR:
				node->SetType( int_type_ );
				break;

			case ILBinary::EQ:
			case ILBinary::NEQ:
			case ILBinary::SGRTR:
			case ILBinary::SGEQ:
			case ILBinary::SLESS:
			case ILBinary::SLEQ:
			case ILBinary::AND:
			case ILBinary::OR:
				node->SetType( bool_type_ );
				break;

			case ILBinary::FLOATADD:
			case ILBinary::FLOATSUB:
			case ILBinary::FLOATMUL:
			case ILBinary::FLOATDIV:
				node->SetType( float_type_ );
				break;

			case ILBinary::FLOATCMP:
			case ILBinary::FLOATGT:
			case ILBinary::FLOATGE:
			case ILBinary::FLOATLE:
			case ILBinary::FLOATLT:
			case ILBinary::FLOATEQ:
			case ILBinary::FLOATNE:
				node->SetType( bool_type_ );
				break;

			default:
				assert( 0 );
				break;
		}

		const SmxVariableType* inner_type = node->left()->type();
		if( !inner_type )
		{
			inner_type = node->right()->type();
		}

		PushType( inner_type );
		Visit( node->left() );
		Visit( node->right() );
		PopType();
	}
	virtual void VisitLocalVar( ILLocalVar* node ) override
	{
		if( !node->type() )
			node->SetType( type() );

		if( node->value() )
		{
			PushType( node->type() );
			Visit( node->value() );
			PopType();
		}
	}
	virtual void VisitGlobalVar( ILGlobalVar* node ) override
	{
		if( !node->type() )
			node->SetType( type() );
	}
	virtual void VisitHeapVar( ILHeapVar* node ) override
	{
		if( !node->type() )
			node->SetType( type() );
	}
	virtual void VisitArrayElementVar( ILArrayElementVar* node ) override
	{
		node->SetType( type() );

		SmxVariableType* arr_type = nullptr;
		if( const SmxVariableType* old_type = type() )
		{
			arr_type = new SmxVariableType( *old_type );

			int* dims = new int[arr_type->dimcount + 1];
			for( int i = 0; i < arr_type->dimcount; i++ )
			{
				dims[i] = old_type->dims[i];
			}
			dims[arr_type->dimcount++] = 0;
			arr_type->dims = dims;
		}

		PushType( arr_type );
		Visit( node->base() );
		PopType();

		PushType( int_type_ );
		Visit( node->index() );
		PopType();
	}
	virtual void VisitTempVar( ILTempVar* node )
	virtual void VisitTempVar( ILTempVar* node ) override
	{
		if( !node->type() )
			node->SetType( type() );
	}
	virtual void VisitLoad( ILLoad* node ) override
	{
		Visit( node->var() );
		node->SetType( node->var()->type() );
	}
	virtual void VisitStore( ILStore* node ) override
	{
		Visit( node->var() );

		const SmxVariableType* var_type = node->var()->type();
		if( var_type && var_type->dimcount > 0 )
		{
			assert( var_type->dimcount == 1 );

			SmxVariableType* inner_type = new SmxVariableType;
			inner_type->tag = var_type->tag;
			var_type = inner_type;
		}

		PushType( var_type );
		Visit( node->val() );
		PopType();
	}
	virtual void VisitJumpCond( ILJumpCond* node ) override
	{
		Visit( node->condition() );
	}
	virtual void VisitSwitch( ILSwitch* node ) override {}
	virtual void VisitCall( ILCall* node ) override
	{
		if( !node->type() )
			node->SetType( type() );
	}
	virtual void VisitNative( ILNative* node ) override
	{
		if( !node->type() )
			node->SetType( type() );
	}
	virtual void VisitReturn( ILReturn* node ) override
	{
		if( node->value() )
		{
			PushType( func_->signature.ret );
			Visit( node->value() );
			PopType();
		}
	}
private:
	const SmxVariableType* type() const { return type_stack_.empty() ? nullptr : type_stack_.back(); }
	void PushType( const SmxVariableType* type ) { type_stack_.push_back( type ); }
	void PopType() { type_stack_.pop_back(); }
private:
	const SmxFunction* func_;
	SmxVariableType* int_type_;
	SmxVariableType* bool_type_;
	SmxVariableType* float_type_;
	std::vector<const SmxVariableType*> type_stack_;
};

void Typer::PopulateTypes( ILControlFlowGraph& cfg )
{
	cell_t pc = cfg.Entry().pc();
	const SmxFunction* func = smx_->FindFunctionAt( pc );

	FillSmxVars( cfg, func );
}

void Typer::FillSmxVars( ILControlFlowGraph& cfg, const SmxFunction* func )
{
	SmxVariableVisitor fill_smx_vars( *smx_, func );
	VisitAllNodes( cfg, fill_smx_vars );
}

void Typer::PropagateTypes( ILControlFlowGraph& cfg )
{
	cell_t pc = cfg.Entry().pc();
	const SmxFunction* func = smx_->FindFunctionAt( pc );

	TypePropagator propagator( func );
	VisitAllNodes( cfg, propagator );
}

void Typer::VisitAllNodes( ILControlFlowGraph& cfg, ILVisitor& visitor )
{
	for( size_t i = 0; i < cfg.num_blocks(); i++ )
	{
		ILBlock& bb = cfg.block( i );
		for( size_t node = 0; node < bb.num_nodes(); node++ )
		{
			bb.node( node )->Accept( &visitor );
		}
	}
}
