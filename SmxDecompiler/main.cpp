#include "smx-file.h"

int main()
{
	SmxFile smx( "tests/test.smx" );
	for( size_t i = 0; i < smx.num_enumerations(); i++ )
	{
		auto& ntv = smx.enumeration( i );
		printf( "Enum: %s\n", ntv.name );
	}

	return 0;
}