#include "smx-file.h"

#include <fstream>
#include <cstdlib>
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

    auto* sections = reinterpret_cast<const sp_file_section_t*>(image_.get() + sizeof( header ));
    for( size_t i = 0; i < header.sections; i++ )
    {
        const sp_file_section_t& section = sections[i];
        ReadSection( stringtab_ + section.nameoffs, section.dataoffs, section.size );
    }
}

void SmxFile::ReadSection( const char* name, size_t offset, size_t size )
{
    if( stricmp( name, ".code" ) == 0 )               ReadCode( name, offset, size );
    else if( stricmp( name, ".data" ) == 0 )          ReadData( name, offset, size );
    else if( stricmp( name, ".names" ) == 0 )         ReadNames( name, offset, size );
    else if( stricmp( name, "rtti.data" ) == 0 )      ReadRttiData( name, offset, size );
    else if( stricmp( name, "rtti.methods" ) == 0 )   ReadRttiMethods( name, offset, size );
}

void SmxFile::ReadCode( const char* name, size_t offset, size_t size )
{
    auto* codehdr = reinterpret_cast<const sp_file_code_t*>(image_.get() + offset);
    code_ = reinterpret_cast<cell_t*>( image_.get() + offset + codehdr->code );
}

void SmxFile::ReadData( const char* name, size_t offset, size_t size )
{
    auto* datahdr = reinterpret_cast<const sp_file_data_t*>(image_.get() + offset);
    data_ = image_.get() + offset + datahdr->data;
}

void SmxFile::ReadNames( const char* name, size_t offset, size_t size )
{
    names_ = image_.get() + offset;
}

void SmxFile::ReadRttiData( const char* name, size_t offset, size_t size )
{
    rtti_data_ = image_.get() + offset;
}

void SmxFile::ReadRttiMethods( const char* name, size_t offset, size_t size )
{
    auto* rttihdr = reinterpret_cast<const smx_rtti_table_header*>(image_.get() + offset);

    for( size_t i = 0; i < rttihdr->row_count; i++ )
    {
        auto* method = reinterpret_cast<const smx_rtti_method*>(image_.get() + offset + rttihdr->header_size + i * rttihdr->row_size);
        SmxFunction func;
        func.name = names_ + method->name;
        func.pcode_start = code_ + method->pcode_start;
        func.pcode_end = code_ + method->pcode_end;
        func.signature.nargs = 0;
        func.signature.varargs = false;
        functions_.push_back( func );

    }
}
