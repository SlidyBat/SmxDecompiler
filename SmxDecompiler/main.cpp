#include <iostream>
#include "smx-file.h"
#include "smx-disasm.h"
#include "cfg-builder.h"
#include "lifter.h"
#include "il-disasm.h"
#include "typer.h"
#include "code-fixer.h"
#include "structurizer.h"
#include "code-writer.h"

int main()
{
	SmxFile smx( "tests/test2.smx" );
	
	for( size_t i = 0; i < smx.num_globals(); i++ )
	{
		SmxVariable& var = smx.global( i );
		CodeWriter writer( smx, "" );
		std::cout << writer.BuildVarDecl( var.name, &var.type ) << ';' << std::endl;
	}
	std::cout << std::endl;

	for( size_t i = 0; i < smx.num_functions(); i++ )
	{
		SmxFunction& func = smx.function( i );
		//if( strcmp( func.name, "AdminMenu_Rename" ) != 0 )
		//	continue;

		//SmxDisassembler disasm( smx );
		//puts( disasm.DisassembleFunction( func ).c_str() );

		CfgBuilder builder( smx );
		ControlFlowGraph cfg = builder.Build( smx.code( func.pcode_start ) );

		PcodeLifter lifter( smx );
		ILControlFlowGraph* ilcfg = lifter.Lift( cfg );

		Typer typer( smx );
		typer.PopulateTypes( *ilcfg );

		CodeFixer fixer( smx );
		fixer.ApplyFixes( *ilcfg );

		Structurizer structurizer( ilcfg );
		Statement* func_stmt = structurizer.Transform();

		//ILDisassembler ildisasm( smx );
		//std::cout << ildisasm.DisassembleCFG( *ilcfg );

		CodeWriter writer( smx, func.name );
		std::string code = writer.Build( func_stmt );
		std::cout << code << std::endl;
	}

	return 0;
}