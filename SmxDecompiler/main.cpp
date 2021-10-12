#include <iostream>
#include "optparse.h"
#include "smx-file.h"
#include "smx-disasm.h"
#include "cfg-builder.h"
#include "lifter.h"
#include "il-disasm.h"
#include "typer.h"
#include "code-fixer.h"
#include "structurizer.h"
#include "code-writer.h"

int main( int argc, const char* argv[] )
{
	OptParse args;
	args.AddArgOption( "function", 'F' )
		.AddFlagOption( "no-globals", 'g' )
		.AddFlagOption( "assembly", 'a' )
		.AddFlagOption( "il", 'i' );
	args.Process( argc, argv );

	if( args.GetArgC() < 1 )
	{
		std::cout << "Usage: "
			<< argv[0]
			<< " [--function/-f <function>] [--no-globals/-g] [--assembly/-a] [--il/-i] <filename>\n";
		return 1;
	}
	SmxFile smx( args.GetArg( 0 ).c_str() );
	
	if( !args["no-globals"] )
	{
		for( size_t i = 0; i < smx.num_globals(); i++ )
		{
			SmxVariable& var = smx.global( i );
			CodeWriter writer( smx, "" );
			std::cout << writer.BuildVarDecl( var.name, &var.type ) << "\n";
		}
		std::cout << std::endl;
	}

	for( size_t i = 0; i < smx.num_functions(); i++ )
	{
		SmxFunction& func = smx.function( i );
		if( args["function"] && strcmp(func.name, args["function"]) != 0 )
			continue;

		if( args["assembly"] )
		{
			SmxDisassembler disasm( smx );
			std::cout << disasm.DisassembleFunction( func ).c_str() << std::endl;
		}

		CfgBuilder builder( smx );
		ControlFlowGraph cfg = builder.Build( smx.code( func.pcode_start ) );

		PcodeLifter lifter( smx );
		ILControlFlowGraph* ilcfg = lifter.Lift( cfg );

		if( args["il"] )
		{
			ILDisassembler ildisasm( smx );
			std::cout << ildisasm.DisassembleCFG( *ilcfg );
		}

		Typer typer( smx );
		typer.PopulateTypes( *ilcfg );

		CodeFixer fixer( smx );
		typer.PopulateTypes( *ilcfg );
		fixer.ApplyFixes( *ilcfg );
		typer.PropagateTypes( *ilcfg );
		fixer.ApplyFixes( *ilcfg );
		typer.PropagateTypes( *ilcfg );
		fixer.ApplyFixes( *ilcfg );

		Structurizer structurizer( ilcfg );
		Statement* func_stmt = structurizer.Transform();

		CodeWriter writer( smx, func.name );
		std::string code = writer.Build( func_stmt );
		std::cout << code << std::endl;
	}

	return 0;
}