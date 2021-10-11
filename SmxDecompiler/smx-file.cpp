#include "smx-file.h"

#include <fstream>
#include <cstdlib>
#include <cassert>
#include "third_party/zlib/zlib.h"

struct SmxConsts {
    static const uint32_t FILE_MAGIC = 0x53504646;

    static const uint16_t SP1_VERSION_1_0 = 0x0101;
    static const uint16_t SP1_VERSION_1_1 = 0x0102;
    static const uint16_t SP1_VERSION_1_7 = 0x0107;
    static const uint16_t SP1_VERSION_MIN = SP1_VERSION_1_0;
    static const uint16_t SP1_VERSION_MAX = SP1_VERSION_1_7;
    static const uint16_t SP2_VERSION_MIN = 0x0200;
    static const uint16_t SP2_VERSION_MAX = 0x0200;

    static const uint8_t FILE_COMPRESSION_NONE = 0;
    static const uint8_t FILE_COMPRESSION_GZ = 1;

    static const uint8_t CODE_VERSION_MINIMUM = 9;
    static const uint8_t CODE_VERSION_SM_LEGACY = 10;
    static const uint8_t CODE_VERSION_FEATURE_MASK = 13;
    static const uint8_t CODE_VERSION_CURRENT = CODE_VERSION_FEATURE_MASK;
    static const uint8_t CODE_VERSION_ALWAYS_REJECT = 0x7f;

    static const uint32_t kCodeFeatureDirectArrays = (1 << 0);
};

// These structures are byte-packed.
#if defined __GNUC__
#    pragma pack(1)
#else
#    pragma pack(push)
#    pragma pack(1)
#endif

typedef struct sp_file_hdr_s {
    uint32_t magic;
    uint16_t version;

    uint8_t compression;
    uint32_t disksize;
    uint32_t imagesize;

    uint8_t sections;

    uint32_t stringtab;

    uint32_t dataoffs;
} sp_file_hdr_t;

typedef struct sp_file_section_s {
    uint32_t nameoffs; /**< Offset into the string table. */
    uint32_t dataoffs; /**< Offset into the file for the section contents. */
    uint32_t size;     /**< Size of this section's contents. */
} sp_file_section_t;

typedef struct sp_file_code_s {
    uint32_t codesize;   /**< Size of the code section. */
    uint8_t cellsize;    /**< Cellsize in bytes. */
    uint8_t codeversion; /**< Version of opcodes supported. */
    uint16_t flags;      /**< Flags. */
    uint32_t main;       /**< Address to "main". */
    uint32_t code;       /**< Offset to bytecode, relative to the start of this section. */

    uint32_t features; /**< List of features flags that this code requires. */
} sp_file_code_t;

typedef struct sp_file_data_s {
    uint32_t datasize; /**< Size of data section in memory */
    uint32_t memsize;  /**< Total mem required (includes data) */
    uint32_t data;     /**< File offset to data (helper) */
} sp_file_data_t;

typedef struct sp_file_publics_s {
    uint32_t address;
    uint32_t name;
} sp_file_publics_t;

typedef struct sp_file_natives_s {
    uint32_t name;
} sp_file_natives_t;

typedef struct sp_file_pubvars_s {
    uint32_t address;
    uint32_t name;
} sp_file_pubvars_t;

struct smx_rtti_table_header {
    uint32_t header_size;
    uint32_t row_size;
    uint32_t row_count;
};

struct smx_rtti_method {
    uint32_t name;
    uint32_t pcode_start;
    uint32_t pcode_end;
    uint32_t signature;
};

struct smx_rtti_native {
    uint32_t name;
    uint32_t signature;
};

struct smx_rtti_enum {
    uint32_t name;
    uint32_t reserved0;
    uint32_t reserved1;
    uint32_t reserved2;
};

struct smx_rtti_typedef {
    uint32_t name;
    uint32_t type_id;
};

struct smx_rtti_typeset {
    uint32_t name;
    uint32_t signature;
};

struct smx_rtti_enumstruct {
    uint32_t name;
    uint32_t first_field;
    uint32_t size;
};

struct smx_rtti_es_field {
    uint32_t name;
    uint32_t type_id;
    uint32_t offset;
};

struct smx_rtti_classdef {
    uint32_t flags;
    uint32_t name;
    uint32_t first_field;

    uint32_t reserved0;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
};

struct smx_rtti_field {
    uint16_t flags;
    uint32_t name;
    uint32_t type_id;
};

struct smx_rtti_debug_method {
    uint32_t method_index;
    uint32_t first_local;
};

struct smx_rtti_debug_var {
    int32_t address;
    uint8_t vclass;
    uint32_t name;
    uint32_t code_start;
    uint32_t code_end;
    uint32_t type_id;
};

static const uint8_t kVarClass_Global = 0x0;
static const uint8_t kVarClass_Local = 0x1;
static const uint8_t kVarClass_Static = 0x2;
static const uint8_t kVarClass_Arg = 0x3;

#if defined __GNUC__
#    pragma pack()
#else
#    pragma pack(pop)
#endif

SmxFile::SmxFile( const char* filename )
{
    std::ifstream file( filename, std::ios::binary );

    sp_file_hdr_t header;
    file.read( reinterpret_cast<char*>(&header), sizeof( header ) );

    if( header.magic != SmxConsts::FILE_MAGIC )
    {
        return;
    }

    file.seekg( 0, std::ios::beg );
    image_ = std::make_unique<char[]>( header.imagesize );

    if( header.compression != SmxConsts::FILE_COMPRESSION_GZ )
    {
        // No compression can just read directly
        file.read( image_.get(), header.imagesize );
    }
    else
    {
        // Read non-compressed section
        file.read( image_.get(), header.dataoffs );

        // Read compressed section into temporary buffer
        auto buffer = std::make_unique<char[]>( header.disksize - header.dataoffs );
        file.read( buffer.get(), header.disksize - header.dataoffs );

        // Decompress into image buffer
        auto* dest = (Bytef*)(image_.get() + header.dataoffs);
        uLongf dest_len = header.imagesize - header.dataoffs;
        auto* source = (Bytef*)buffer.get();
        uLongf source_len = header.disksize - header.dataoffs;
        int rv = uncompress( dest, &dest_len, source, source_len );
        if( rv != Z_OK )
        {
            return;
        }
    }

    stringtab_ = image_.get() + header.stringtab;

    auto* sp_sections = reinterpret_cast<const sp_file_section_t*>(image_.get() + sizeof( header ));
    sections_.reserve( header.sections );
    for( size_t i = 0; i < header.sections; i++ )
    {
        const sp_file_section_t& sp_section = sp_sections[i];
        SmxSection section;
        section.name = stringtab_ + sp_section.nameoffs;
        section.offset = sp_section.dataoffs;
        section.size = sp_section.size;
        sections_.push_back( section );
    }

    ReadSections();
}

SmxFunction* SmxFile::FindFunctionByName( const char* func_name )
{
    for( SmxFunction& func : functions_ )
    {
        if( strcmp( func.name, func_name ) == 0 )
        {
            return &func;
        }
    }
    return nullptr;
}

SmxFunction* SmxFile::FindFunctionAt( cell_t addr )
{
    for( SmxFunction& func : functions_ )
    {
        if( addr >= func.pcode_start && addr < func.pcode_end )
        {
            return &func;
        }
    }
    return nullptr;
}

SmxNative* SmxFile::FindNativeByIndex( size_t index )
{
    if( index < natives_.size() )
    {
        return &natives_[index];
    }
    return nullptr;
}

SmxVariable* SmxFile::FindGlobalByName( const char* var_name )
{
    for( SmxVariable& var : globals_ )
    {
        if( strcmp( var.name, var_name ) == 0 )
        {
            return &var;
        }
    }
    return nullptr;
}

SmxVariable* SmxFile::FindGlobalAt( cell_t addr )
{
    for( SmxVariable& var : globals_ )
    {
        if( var.address == addr )
        {
            return &var;
        }
    }
    return nullptr;
}

SmxSection* SmxFile::GetSectionByName( const char* name )
{
    for( SmxSection& section : sections_ )
    {
        if( stricmp( name, section.name ) == 0 )
        {
            return &section;
        }
    }
    return nullptr;
}

#define READ_SECTION( sec_name, handler ) \
    do { \
        SmxSection* section = GetSectionByName( sec_name ); \
        if( section ) handler( section->name, section->offset, section->size ); \
    } while( false )
void SmxFile::ReadSections()
{
    READ_SECTION( ".code",                  ReadCode );
    READ_SECTION( ".data",                  ReadData );
    READ_SECTION( ".names",                 ReadNames );
    READ_SECTION( "rtti.data",              ReadRttiData );
    READ_SECTION( "rtti.enums",             ReadRttiEnums );
    READ_SECTION( "rtti.typedefs",          ReadRttiTypeDefs );
    READ_SECTION( "rtti.typesets",          ReadRttiTypeSets );
    READ_SECTION( "rtti.fields",            ReadRttiFields );
    READ_SECTION( "rtti.classdefs",         ReadRttiClassdefs );
    READ_SECTION( "rtti.enumstruct_fields", ReadRttiEnumStructFields );
    READ_SECTION( "rtti.enumstructs",       ReadRttiEnumStructs );
    READ_SECTION( "rtti.methods",           ReadRttiMethods );
    READ_SECTION( "rtti.natives",           ReadRttiNatives );
    READ_SECTION( ".dbg.globals",           ReadDbgGlobals );
    READ_SECTION( ".dbg.locals",            ReadDbgLocals );
    READ_SECTION( ".dbg.methods",           ReadDbgMethods );
    READ_SECTION( ".publics",               ReadPublics );
    READ_SECTION( ".pubvars",               ReadPubvars );
    READ_SECTION( ".natives",               ReadNatives );
    // TODO: Also read legacy debug sections (.dbg.symbols, .dbg.natives)
}
#undef READ_SECTION

void SmxFile::ReadCode( const char* name, size_t offset, size_t size )
{
    auto* codehdr = reinterpret_cast<const sp_file_code_t*>(image_.get() + offset);
    code_ = reinterpret_cast<cell_t*>( image_.get() + offset + codehdr->code );
    code_size_ = codehdr->codesize;
}

void SmxFile::ReadData( const char* name, size_t offset, size_t size )
{
    auto* datahdr = reinterpret_cast<const sp_file_data_t*>(image_.get() + offset);
    data_ = image_.get() + offset + datahdr->data;
    data_size_ = datahdr->datasize;
}

void SmxFile::ReadNames( const char* name, size_t offset, size_t size )
{
    names_ = image_.get() + offset;
}

void SmxFile::ReadPublics( const char* name, size_t offset, size_t size )
{
    // Already filled by .rtti.methods
    if( !functions_.empty() )
        return;

    auto* rows = (sp_file_publics_t*)(image_.get() + offset);
    size_t row_count = size / sizeof( sp_file_publics_t );
    for( size_t i = 0; i < row_count; i++ )
    {
        SmxFunction func;
        func.name = names_ + rows[i].name;
        func.pcode_start = rows[i].address;
        // No pcode_end for the .publics section, just set it to end of .code section
        func.pcode_end = code_size();

        functions_.push_back( func );
    }
}

void SmxFile::ReadPubvars( const char* name, size_t offset, size_t size )
{
    // Already filled by .dbg.globals
    if( !globals_.empty() )
        return;

    auto* rows = (sp_file_pubvars_t*)(image_.get() + offset);
    size_t row_count = size / sizeof( sp_file_pubvars_t );
    for( size_t i = 0; i < row_count; i++ )
    {
        SmxVariable global;
        global.name = names_ + rows[i].name;
        global.address = rows[i].address;
        global.vclass = SmxVariableClass::GLOBAL;

        globals_.push_back( global );
    }
}

void SmxFile::ReadNatives( const char* name, size_t offset, size_t size )
{
    // Already filled by .rtti.natives
    if( !natives_.empty() )
        return;

    auto* rows = (sp_file_natives_t*)(image_.get() + offset);
    size_t row_count = size / sizeof( sp_file_natives_t );
    for( size_t i = 0; i < row_count; i++ )
    {
        SmxNative native;
        native.name = names_ + rows[i].name;

        natives_.push_back( native );
    }
}

void SmxFile::ReadRttiData( const char* name, size_t offset, size_t size )
{
    rtti_data_ = (unsigned char*)image_.get() + offset;
}

void SmxFile::ReadRttiMethods( const char* name, size_t offset, size_t size )
{
    auto* rttihdr = reinterpret_cast<const smx_rtti_table_header*>(image_.get() + offset);

    functions_.reserve( rttihdr->row_count );
    for( size_t i = 0; i < rttihdr->row_count; i++ )
    {
        auto* row = reinterpret_cast<const smx_rtti_method*>(image_.get() + offset + rttihdr->header_size + i * rttihdr->row_size);
        SmxFunction func;
        func.name = names_ + row->name;
        func.pcode_start = row->pcode_start;
        func.pcode_end = row->pcode_end;
        func.signature = DecodeFunctionSignature( row->signature );
        functions_.push_back( func );
    }
}

void SmxFile::ReadRttiNatives( const char* name, size_t offset, size_t size )
{
    auto* rttihdr = reinterpret_cast<const smx_rtti_table_header*>(image_.get() + offset);

    natives_.reserve( rttihdr->row_count );
    for( size_t i = 0; i < rttihdr->row_count; i++ )
    {
        auto* row = reinterpret_cast<const smx_rtti_native*>(image_.get() + offset + rttihdr->header_size + i * rttihdr->row_size);
        SmxNative ntv;
        ntv.name = names_ + row->name;
        ntv.signature = DecodeFunctionSignature( row->signature );
        natives_.push_back( ntv );
    }
}

void SmxFile::ReadRttiEnums( const char* name, size_t offset, size_t size )
{
    auto* rttihdr = reinterpret_cast<const smx_rtti_table_header*>(image_.get() + offset);

    enums_.reserve( rttihdr->row_count );
    for( size_t i = 0; i < rttihdr->row_count; i++ )
    {
        auto* row = reinterpret_cast<const smx_rtti_enum*>(image_.get() + offset + rttihdr->header_size + i * rttihdr->row_size);
        SmxEnum enm;
        enm.name = names_ + row->name;
        enums_.push_back( enm );
    }
}

void SmxFile::ReadRttiTypeDefs( const char* name, size_t offset, size_t size )
{
    auto* rttihdr = reinterpret_cast<const smx_rtti_table_header*>( image_.get() + offset );

    typedefs_.reserve( rttihdr->row_count );
    for( size_t i = 0; i < rttihdr->row_count; i++ )
    {
        auto* row = reinterpret_cast<const smx_rtti_typedef*>( image_.get() + offset + rttihdr->header_size + i * rttihdr->row_size );
        SmxTypeDef td;
        td.name = names_ + row->name;
        typedefs_.push_back( td );
    }
}

void SmxFile::ReadRttiTypeSets( const char* name, size_t offset, size_t size )
{
    auto* rttihdr = reinterpret_cast<const smx_rtti_table_header*>( image_.get() + offset );

    typesets_.reserve( rttihdr->row_count );
    for( size_t i = 0; i < rttihdr->row_count; i++ )
    {
        auto* row = reinterpret_cast<const smx_rtti_typeset*>( image_.get() + offset + rttihdr->header_size + i * rttihdr->row_size );
        SmxTypeSet ts;
        ts.name = names_ + row->name;
        typesets_.push_back( std::move( ts ) );
    }
}

void SmxFile::ReadRttiClassdefs( const char* name, size_t offset, size_t size )
{
    auto* rttihdr = reinterpret_cast<const smx_rtti_table_header*>(image_.get() + offset);

    classdefs_.reserve( rttihdr->row_count );
    for( size_t i = 0; i < rttihdr->row_count; i++ )
    {
        auto* row = reinterpret_cast<const smx_rtti_classdef*>(image_.get() + offset + rttihdr->header_size + i * rttihdr->row_size);
        SmxClassDef classdef;
        classdef.flags = row->flags;
        classdef.name = names_ + row->name;
        if( i < rttihdr->row_count - 1 )
        {
            auto* next_row = reinterpret_cast<const smx_rtti_classdef*>(image_.get() + offset + rttihdr->header_size + (i + 1) * rttihdr->row_size);
            classdef.num_fields = next_row->first_field - row->first_field;
        }
        else
        {
            classdef.num_fields = fields_.size() - row->first_field;
        }
        classdef.fields = &fields_[row->first_field];
        classdefs_.push_back( classdef );
    }
}

void SmxFile::ReadRttiFields( const char* name, size_t offset, size_t size )
{
    auto* rttihdr = reinterpret_cast<const smx_rtti_table_header*>(image_.get() + offset);

    fields_.reserve( rttihdr->row_count );
    for( size_t i = 0; i < rttihdr->row_count; i++ )
    {
        auto* row = reinterpret_cast<const smx_rtti_field*>(image_.get() + offset + rttihdr->header_size + i * rttihdr->row_size);
        SmxField field;
        field.name = names_ + row->name;
        field.type = DecodeVariableType( row->type_id );
        fields_.push_back( field );
    }
}

void SmxFile::ReadRttiEnumStructs( const char* name, size_t offset, size_t size )
{
    auto* rttihdr = reinterpret_cast<const smx_rtti_table_header*>( image_.get() + offset );

    enum_structs_.reserve( rttihdr->row_count );
    for( size_t i = 0; i < rttihdr->row_count; i++ )
    {
        auto* row = reinterpret_cast<const smx_rtti_enumstruct*>( image_.get() + offset + rttihdr->header_size + i * rttihdr->row_size );
        SmxEnumStruct es;
        es.name = names_ + row->name;
        if( i < rttihdr->row_count - 1 )
        {
            auto* next_row = reinterpret_cast<const smx_rtti_enumstruct*>( image_.get() + offset + rttihdr->header_size + (i + 1) * rttihdr->row_size );
            es.num_fields = next_row->first_field - row->first_field;
        }
        else
        {
            es.num_fields = es_fields_.size() - row->first_field;
        }
        es.fields = &es_fields_[row->first_field];
        es.size = row->size;
        enum_structs_.push_back( es );
    }
}

void SmxFile::ReadRttiEnumStructFields( const char* name, size_t offset, size_t size )
{
    auto* rttihdr = reinterpret_cast<const smx_rtti_table_header*>( image_.get() + offset );

    es_fields_.reserve( rttihdr->row_count );
    for( size_t i = 0; i < rttihdr->row_count; i++ )
    {
        auto* row = reinterpret_cast<const smx_rtti_es_field*>( image_.get() + offset + rttihdr->header_size + i * rttihdr->row_size );
        SmxESField esf;
        esf.name = names_ + row->name;
        esf.type = DecodeVariableType( row->type_id );
        esf.offset = row->offset;
        es_fields_.push_back( esf );
    }
}

void SmxFile::ReadDbgMethods( const char* name, size_t offset, size_t size )
{
    auto* rttihdr = reinterpret_cast<const smx_rtti_table_header*>( image_.get() + offset );

    for( size_t i = 0; i < rttihdr->row_count; i++ )
    {
        auto* row = reinterpret_cast<const smx_rtti_debug_method*>( image_.get() + offset + rttihdr->header_size + i * rttihdr->row_size );
        SmxFunction& func = functions_[row->method_index];
        if( i != rttihdr->row_count - 1 )
        {
            auto* next_row = reinterpret_cast<const smx_rtti_debug_method*>( image_.get() + offset + rttihdr->header_size + (i + 1) * rttihdr->row_size );
            func.num_locals = next_row->first_local - row->first_local;
        }
        else
        {
            func.num_locals = locals_.size() - row->first_local;
        }
        func.locals = &locals_[row->first_local];

        // Now that we have locals info, fill in names in signatures
        for( size_t arg = 0; arg < func.signature.nargs; arg++ )
        {
            SmxVariable* arg_local = func.FindLocalByStackOffset( arg * 4 + 12 );
            if( !arg_local )
                continue;
            assert( arg_local->vclass == SmxVariableClass::ARG );
            func.signature.args[arg].name = arg_local->name;
        }
    }
}

void SmxFile::ReadDbgGlobals( const char* name, size_t offset, size_t size )
{
    auto* rttihdr = reinterpret_cast<const smx_rtti_table_header*>( image_.get() + offset );

    globals_.reserve( rttihdr->row_count );
    for( size_t i = 0; i < rttihdr->row_count; i++ )
    {
        auto* row = reinterpret_cast<const smx_rtti_debug_var*>( image_.get() + offset + rttihdr->header_size + i * rttihdr->row_size );
        SmxVariable var;
        var.name = names_ + row->name;
        var.address = row->address;
        var.type = DecodeVariableType( row->type_id );
        var.vclass = (SmxVariableClass)row->vclass;
        globals_.push_back( var );
    }
}

void SmxFile::ReadDbgLocals( const char* name, size_t offset, size_t size )
{
    auto* rttihdr = reinterpret_cast<const smx_rtti_table_header*>( image_.get() + offset );

    locals_.reserve( rttihdr->row_count );
    for( size_t i = 0; i < rttihdr->row_count; i++ )
    {
        auto* row = reinterpret_cast<const smx_rtti_debug_var*>( image_.get() + offset + rttihdr->header_size + i * rttihdr->row_size );
        SmxVariable var;
        var.name = names_ + row->name;
        var.address = row->address;
        var.type = DecodeVariableType( row->type_id );
        var.vclass = (SmxVariableClass)row->vclass;
        locals_.push_back( var );
    }
}

// These are control bytes for type signatures.
//
// uint32 values are encoded with a variable length encoding:
//   0x00  - 0x7f:    1 byte
//   0x80  - 0x7fff:  2 bytes
//   0x8000 - 0x7fffff: 3 bytes
//   0x800000 - 0x7fffffff: 4 bytes
//   0x80000000 - 0xffffffff: 5 bytes
namespace cb {

    // This section encodes raw types.
    static const uint8_t kBool = 0x01;
    static const uint8_t kInt32 = 0x06;
    static const uint8_t kFloat32 = 0x0c;
    static const uint8_t kChar8 = 0x0e;
    static const uint8_t kAny = 0x10;
    static const uint8_t kTopFunction = 0x11;

    // This section encodes multi-byte raw types.

    // kFixedArray is followed by:
    //    Size          uint32
    //    Type          <type>
    //
    // kArray is followed by:
    //    Type          <type>
    static const uint8_t kFixedArray = 0x30;
    static const uint8_t kArray = 0x31;

    // kFunction is always followed by the same encoding as in
    // smx_rtti_method::signature.
    static const uint8_t kFunction = 0x32;

    // Each of these is followed by an index into an appropriate table.
    static const uint8_t kEnum = 0x42;       // rtti.enums
    static const uint8_t kTypedef = 0x43;    // rtti.typedefs
    static const uint8_t kTypeset = 0x44;    // rtti.typesets
    static const uint8_t kClassdef = 0x45;   // rtti.classdefs
    static const uint8_t kEnumStruct = 0x46; // rtti.enumstructs

    // This section encodes special indicator bytes that can appear within multi-
    // byte types.

    // For function signatures, indicating no return value.
    static const uint8_t kVoid = 0x70;
    // For functions, indicating the last argument of a function is variadic.
    static const uint8_t kVariadic = 0x71;
    // For parameters, indicating pass-by-ref.
    static const uint8_t kByRef = 0x72;
    // For reference and compound types, indicating const.
    static const uint8_t kConst = 0x73;

} // namespace cb

// A type identifier is a 32-bit value encoding a type. It is encoded as
// follows:
//   bits 0-3:  type kind
//   bits 4-31: type payload
//
// The kind is a type signature that can be completely inlined in the
// remaining 28 bits.
static const uint8_t kTypeId_Inline = 0x0;
// The payload is an index into the rtti.data section.
static const uint8_t kTypeId_Complex = 0x1;

static const uint32_t kMaxTypeIdPayload = 0xfffffff;
static const uint32_t kMaxTypeIdKind = 0xf;

SmxVariableType SmxFile::DecodeVariableType( uint32_t type_id )
{
    uint8_t kind = type_id & 0b1111;
    uint32_t payload = type_id >> 4;

    unsigned char* data;
    if( kind == kTypeId_Inline )
    {
        data = (unsigned char*)&payload;
    }
    else
    {
        data = &rtti_data_[payload];
    }

    return DecodeVariableType( &data );
}

SmxVariableType SmxFile::DecodeVariableType( unsigned char** data )
{
    SmxVariableType type;

    unsigned char*& d = *data;

    if( *d == cb::kConst )
    {
        type.flags |= SmxVariableType::IS_CONST;
        d++;
    }

    char b = *d++;
    switch( b )
    {
        case cb::kBool:
            type.tag = SmxVariableType::BOOL;
            break;
        case cb::kInt32:
            type.tag = SmxVariableType::INT;
            break;
        case cb::kFloat32:
            type.tag = SmxVariableType::FLOAT;
            break;
        case cb::kChar8:
            type.tag = SmxVariableType::CHAR;
            break;
        case cb::kAny:
            type.tag = SmxVariableType::ANY;
            break;

        case cb::kArray:
        {
            SmxVariableType inner = DecodeVariableType( data );
            type.dimcount = inner.dimcount + 1;
            int* dims = new int[type.dimcount];
            dims[0] = 0;
            for( int i = 0; i < inner.dimcount; i++ )
            {
                dims[i + 1] = inner.dims[i];
            }
            type.dims = dims;
            type.tag = inner.tag;
            break;
        }
        case cb::kFixedArray:
        {
            int size = DecodeUint32( data );
            SmxVariableType inner = DecodeVariableType( data );
            type = inner;
            type.dimcount++;
            int* dims = new int[type.dimcount];
            dims[0] = size;
            for( int i = 0; i < inner.dimcount; i++ )
            {
                dims[i + 1] = inner.dims[i];
            }
            type.dims = dims;
            break;
        }

        case cb::kFunction:
            break;

        case cb::kEnum:
        {
            uint32_t index = DecodeUint32( &d );
            type.tag = SmxVariableType::ENUM;
            type.enumeration = &enums_[index];
            break;
        }
        case cb::kTypedef:
        {
            uint32_t index = DecodeUint32( &d );
            type.tag = SmxVariableType::TYPEDEF;
            type.type_def = &typedefs_[index];
            break;
        }
        case cb::kTypeset:
        {
            uint32_t index = DecodeUint32( &d );
            type.tag = SmxVariableType::TYPESET;
            type.type_set = &typesets_[index];
            break;
        }
        case cb::kClassdef:
        {
            uint32_t index = DecodeUint32( &d );
            type.tag = SmxVariableType::CLASSDEF;
            type.classdef = &classdefs_[index];
            break;
        }
        case cb::kEnumStruct:
        {
            uint32_t index = DecodeUint32( &d );
            type.tag = SmxVariableType::ENUM_STRUCT;
            type.enum_struct = &enum_structs_[index];
            break;
        }
    }

    return type;
}

SmxFunctionSignature SmxFile::DecodeFunctionSignature( uint32_t signature )
{
    unsigned char* data = &rtti_data_[signature];
    return DecodeFunctionSignature( &data );
}

SmxFunctionSignature SmxFile::DecodeFunctionSignature( unsigned char** data )
{
    unsigned char*& d = *data;
    SmxFunctionSignature sig;

    sig.nargs = *d++;

    sig.varargs = false;
    if( *d == cb::kVariadic )
    {
        sig.varargs = true;
        d++;
    }

    if( *d == cb::kVoid )
    {
        sig.ret = new SmxVariableType;
        sig.ret->tag = SmxVariableType::VOID;
        d++;
    }
    else
    {
        sig.ret = new SmxVariableType( DecodeVariableType( &d ) );
    }

    sig.args = new SmxFunctionSignature::SmxFunctionSignatureArg[sig.nargs];
    for( size_t i = 0; i < sig.nargs; i++ )
    {
        bool by_ref = false;
        if( *d == cb::kByRef )
        {
            by_ref = true;
            d++;
        }

        sig.args[i].type = DecodeVariableType( &d );
        if( by_ref )
            sig.args[i].type.flags |= SmxVariableType::BY_REF;
    }

    return sig;
}

uint32_t SmxFile::DecodeUint32( unsigned char** data )
{
    unsigned char*& d = *data;

    uint32_t value = 0;
    uint32_t shift = 0;
    while( true )
    {
        char b = *d++;
        value |= (b & 0x7f) << shift;
        if( (b & 0x80) == 0 )
            break;
        shift += 7;
    }
    return value;
}
