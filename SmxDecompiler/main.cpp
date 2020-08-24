#include "smx-file.h"

int main()
{
	SmxFile smx( "tests/test.smx" );
	for( size_t i = 0; i < smx.num_enum_structs(); i++ )
	{
		auto& es = smx.enum_struct( i );
		printf( "Enum: %s (size=%i)\n", es.name, es.size );
		for( int field = 0; field < es.num_fields; field++ )
		{
			printf( "\tField: %s (offset=%i)\n", es.fields[field].name, es.fields[field].offset );
		}
	}

	return 0;
}