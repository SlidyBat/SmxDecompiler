#include <iostream>
#include "smx-file.h"
#include "smx-disasm.h"
#include "cfg-builder.h"
#include "lifter.h"
#include "il-disasm.h"
#include "structurizer.h"
#include "code-writer.h"

int main()
{
	SmxFile smx( "tests/test2.smx" );
	
	for( size_t i = 0; i < smx.num_globals(); i++ )
	{
		SmxVariable& var = smx.global( i );
		CodeWriter writer( smx, "" );
		std::cout << writer.Type( var.type ) << " " << var.name << ';' << std::endl;
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

		Structurizer structurizer( ilcfg );
		Statement* func_stmt = structurizer.Transform();

		CodeWriter writer( smx, func.name );
		std::string code = writer.Build( func_stmt );
		std::cout << code << std::endl;
	}

	return 0;
}