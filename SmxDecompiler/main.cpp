#include "smx-file.h"

int main()
{
	SmxFile smx( "tests/test.smx" );
	for( size_t i = 0; i < smx.num_functions(); i++ )
	{
		SmxFunction& func = smx.function( i );
		printf( "Function: %s\n", func.name );
	}

	return 0;
}