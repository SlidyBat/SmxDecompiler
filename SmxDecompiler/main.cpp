#include "smx-file.h"
#include "smx-disasm.h"

int main()
{
	SmxFile smx( "tests/test.smx" );
	SmxFunction& func = smx.function( 0 );
	std::string disasm = SmxDisassembler::DisassembleFunction( func );
	puts( disasm.c_str() );

	return 0;
}