#include "code-fixer.h"

#include "il.h"

// Sometimes globals are referenced by their constant address.
// To the lifter, this looks like just a regular constant, but
// it should be interpreted as a variable.
// 
// Before:
//  `2332[i] = 0`
// After:
//  `g_var[i] = 0`
// 
class FixConstGlobals : public RecursiveILVisitor
{
public:
	virtual void VisitArrayElementVar( ILArrayElementVar* node ) override
	{
		if( auto* constant = dynamic_cast<ILConst*>(node->base()) )
		{
			auto* var = new ILGlobalVar( constant->value() );
			constant->ReplaceUsesWith( var );
		}
	}
};

// When arrays are loaded from / stored into at index 0, there is no indexing operation.
// This class fixes up those cases to add in the index into first element. It also fixes
// up cases where addition is used to index into an array.
// 
// Before:
//  `arr = x`
//  `x = arr`
//  `PrintToServer("%s", arr + i)`
// After:
//  `arr[0] = x`
//  `x = arr[0]`
//  `PrintToServer("%s", arr[i])`
//
class FixArrays : public RecursiveILVisitor
{
public:
	virtual void VisitLoad( ILLoad* node ) override
	{
		const SmxVariableType* type = node->var()->type();
		
		// Not much we can do without type information at this stage, just bail
		if( !type )
			return;

		// Not an array or already an index operation, nothing to fix
		if( type->dimcount == 0 || dynamic_cast<ILArrayElementVar*>(node->var()) )
			return;

		ILArrayElementVar* new_var = new ILArrayElementVar( node->var(), new ILConst( 0 ) );
		node->ReplaceParam( node->var(), new_var );
	}
	virtual void VisitStore( ILStore* node ) override
	{
		const SmxVariableType* type = node->var()->type();

		// Not much we can do without type information at this stage, just bail
		if( !type )
			return;

		// Not an array or already an index operation, nothing to fix
		if( type->dimcount == 0 || dynamic_cast<ILArrayElementVar*>(node->var()) )
			return;

		ILArrayElementVar* new_var = new ILArrayElementVar( node->var(), new ILConst( 0 ) );
		node->ReplaceParam( node->var(), new_var );
	}
	virtual void VisitBinary( ILBinary* node ) override
	{
		if( node->op() == ILBinary::ADD )
		{
			ILVar* base = nullptr;
			ILNode* index = nullptr;
			if( ILVar* var = dynamic_cast<ILVar*>(node->left()) )
			{
				if( var->type() && var->type()->dimcount > 0 )
				{
					base = var;
					index = node->right();
				}
			}
			if( ILVar* var = dynamic_cast<ILVar*>(node->right()) )
			{
				if( var->type() && var->type()->dimcount > 0 )
				{
					base = var;
					index = node->left();
				}
			}

			if( !index || !base )
				return;

			node->ReplaceUsesWith( new ILArrayElementVar( base, index ) );
		}
	}
};

// Some float operations are implemented via natives rather than actual instructions.
// This replaces those natives with actual instructions.
// 
// Before:
//  `c = FloatAdd(a, b)`
// After:
//  `c = a + b`
//
class ReplaceFloatNatives : public RecursiveILVisitor
{
public:
	ReplaceFloatNatives( SmxFile& smx ) : smx_( &smx ) {}

	virtual void VisitNative( ILNative* node ) override
	{
		// Handle the args of this call, they can contain native calls that we want to replace too
		RecursiveILVisitor::VisitNative( node );

		SmxNative* native = smx_->FindNativeByIndex( node->native_index() );
		assert( native );

		if( strcmp( native->name, "FloatMul" ) == 0 || strcmp( native->name, "__FLOAT_MUL__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATMUL, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "FloatDiv" ) == 0 || strcmp( native->name, "__FLOAT_DIV__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATDIV, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "FloatAdd" ) == 0 || strcmp( native->name, "__FLOAT_ADD__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATADD, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "FloatSub" ) == 0 || strcmp( native->name, "__FLOAT_SUB__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATSUB, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "__FLOAT_NOT__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILUnary( node->arg( 0 ), ILUnary::FLOATNOT ) );
		}
		else if( strcmp( native->name, "__FLOAT_GT__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATGT, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "__FLOAT_GE__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATGE, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "__FLOAT_LT__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATLT, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "__FLOAT_LE__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATLE, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "__FLOAT_NE__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATNE, node->arg( 1 ) ) );
		}
		else if( strcmp( native->name, "__FLOAT_EQ__" ) == 0 )
		{
			node->ReplaceUsesWith( new ILBinary( node->arg( 0 ), ILBinary::FLOATEQ, node->arg( 1 ) ) );
		}
	}
private:
	SmxFile* smx_;
};

// If the function has a void return type, then return statements shouldn't have value
// 
// Before:
//  `return x`
// After:
//  `return`
//
class RemoveVoidRets : public ILVisitor
{
public:
	void VisitReturn( ILReturn* node )
	{
		if( node->value() )
			node->ReplaceParam( node->value(), nullptr );
	}
};

// Bool operations are always represented with comparisons to zero, which should be implicit anyways
//
// Before:
//  `if ((x == 2) != 0) {}`
//  `if ((x > 0) == 0) {}`
// After:
//  `if (x == 2) {}`
//  `if (!(x > 0)) {}`
//
class UseBoolOps : public RecursiveILVisitor
{
public:
	void VisitBinary( ILBinary* node )
	{
		if( node->op() != ILBinary::EQ && node->op() != ILBinary::NEQ )
			return;

		if( !(node->left()->type() && node->left()->type()->tag == SmxVariableType::BOOL) )
			return;

		if( auto* constant = dynamic_cast<ILConst*>(node->right()) )
		{
			if( constant->value() != 0 )
				return;

			if( node->op() == ILBinary::EQ )
			{
				node->ReplaceUsesWith( new ILUnary( node->left(), ILUnary::NOT ) );
			}
			else
			{
				node->ReplaceUsesWith( node->left() );
			}
		}
	}
};

void CodeFixer::ApplyFixes( ILControlFlowGraph& cfg ) const
{
	FixConstGlobals fix_const_globals;
	VisitAllNodes( cfg, fix_const_globals );

	FixArrays arrays;
	VisitAllNodes( cfg, arrays );

	ReplaceFloatNatives replace_float_natives( *smx_ );
	VisitAllNodes( cfg, replace_float_natives );

	SmxFunction* func = smx_->FindFunctionAt( cfg.Entry().pc() );
	if( func->signature.ret && func->signature.ret->tag == SmxVariableType::VOID )
	{
		RemoveVoidRets remove_void_rets;
		VisitAllNodes( cfg, remove_void_rets );
	}

	UseBoolOps use_bool_ops;
	VisitAllNodes( cfg, use_bool_ops );

	for( size_t i = 0; i < cfg.num_blocks(); i++ )
	{
		CleanStores( cfg.block( i ) );
		CleanIncAndDec( cfg.block( i ) );
		RemoveTmpLocalVars( cfg.block( i ) );
	}
}

void CodeFixer::VisitAllNodes( ILControlFlowGraph& cfg, ILVisitor& visitor ) const
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

void CodeFixer::CleanStores( ILBlock& bb ) const
{
	// There is a common pattern of declaring a local variable then storing a value into it
	// e.g.
	// ```
	// float x;
	// x = a + b;
	// ```
	//
	// We can clean this up by removing the store and placing its value into the declaration
	// e.g.
	// ```
	// float x = a + b;
	//
	for( int i = (int)bb.num_nodes() - 1; i >= 1; i-- )
	{
		if( auto* store = dynamic_cast<ILStore*>(bb.node( i )) )
		{
			if( auto* store_var = dynamic_cast<ILLocalVar*>(store->var()) )
			{
				if( auto* decl_var = dynamic_cast<ILLocalVar*>(bb.node( i - 1 )) )
				{
					if( store_var == decl_var && decl_var->value() == nullptr )
					{
						decl_var->SetValue( store->val() );
						bb.Remove( i );
					}
				}
			}
		}
	}
}

void CodeFixer::CleanIncAndDec( ILBlock& bb ) const
{
	// INC/DEC instructions should not be a part of a store since the
	// store is implicit, however in the pcode the store is explicit
	//
	// This results in code that looks like this:
	// `i = ++i`
	// 
	// When we actually want:
	// `++i`
	//
	for( int i = (int)bb.num_nodes() - 1; i >= 0; i-- )
	{
		if( auto* store = dynamic_cast<ILStore*>(bb.node( i )) )
		{
			if( auto* unary = dynamic_cast<ILUnary*>(store->val()) )
			{
				if( unary->op() == ILUnary::INC || unary->op() == ILUnary::DEC )
				{
					bb.Replace( i, unary );
				}
			}
		}
	}
}

void CodeFixer::RemoveTmpLocalVars( ILBlock& bb ) const
{
	// Sometimes local vars are used to store result from other var.
	// This pass will optimize out those local vars and use the result immediately.
	// Before:
	//  `int local_3212 = x;`
	//  `PrintToServer("%i", local_3212);`
	// After:
	//  `PrintToServer("%i", x);`
	// 
	// This could result in optimizing out code that was part of original source, but
	// that shouldn't affect readability of the decompilation. As a way to try avoiding
	// this case, only variables that don't have any debug info associated with them are
	// removed.
	//
	for( int i = (int)bb.num_nodes() - 1; i >= 0; i-- )
	{
		if( auto* local_var = dynamic_cast<ILLocalVar*>(bb.node( i )) )
		{
			if( local_var->smx_var() )
				continue;

			if( !local_var->value() || local_var->num_uses() > 1 )
				continue;

			local_var->ReplaceUsesWith( local_var->value() );
			bb.Remove( i );
		}
	}
}
