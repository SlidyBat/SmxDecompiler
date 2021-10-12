#pragma once

#include <vector>
#include <memory>

using cell_t = int32_t;

struct SmxSection
{
	const char* name;
	size_t offset;
	size_t size;
};

struct SmxVariableType
{
	enum SmxVariableTag
	{
		UNKNOWN = 0,
		VOID,
		BOOL,
		INT,
		FLOAT,
		CHAR,
		ANY,

		ENUM,
		TYPEDEF,
		TYPESET,
		CLASSDEF,
		ENUM_STRUCT
	};
	SmxVariableTag tag = UNKNOWN;

	const int* dims = nullptr;
	int dimcount = 0;

	union
	{
		struct SmxEnum* enumeration;
		struct SmxTypeDef* type_def;
		struct SmxTypeSet* type_set;
		struct SmxEnumStruct* enum_struct;
		struct SmxClassDef* classdef;
	};

	enum SmxVariableTypeFlags
	{
		NONE = 0,
		IS_CONST = 1,
		BY_REF = 2
	};
	int flags = SmxVariableTypeFlags::NONE;
};

struct SmxFunctionSignature
{
	SmxVariableType* ret = nullptr; // nullptr if void
	struct SmxFunctionSignatureArg
	{
		const char* name = nullptr;
		SmxVariableType type;
	};
	SmxFunctionSignatureArg* args = nullptr;

	size_t nargs = 0;
	bool varargs = false;
};

enum class SmxVariableClass
{
	GLOBAL = 0,
	LOCAL = 1,
	STATIC = 2,
	ARG = 3
};

struct SmxVariable
{
	const char* name;
	cell_t address;
	SmxVariableType type;
	SmxVariableClass vclass;
	bool is_public;
};

struct SmxFunction
{
	const char* raw_name;
	const char* name;
	cell_t pcode_start;
	cell_t pcode_end;
	bool is_public;
	SmxFunctionSignature signature;
	size_t num_locals = 0;
	SmxVariable* locals;

	SmxVariable* FindLocalByStackOffset( int stack_offset )
	{
		for( size_t i = 0; i < num_locals; i++ )
		{
			if( locals[i].address == stack_offset )
				return &locals[i];
		}
		return nullptr;
	}
};

struct SmxNative
{
	const char* name;
	SmxFunctionSignature signature;
};

struct SmxEnum
{
	const char* name;
};

struct SmxTypeDef
{
	const char* name;
	SmxFunctionSignature signature;
};

struct SmxTypeSet
{
	const char* name;
	int num_signatures;
	std::unique_ptr<SmxFunctionSignature[]> signatures;
};

struct SmxESField
{
	const char* name;
	SmxVariableType type;
	uint32_t offset;
};

struct SmxEnumStruct
{
	const char* name;
	size_t num_fields;
	SmxESField* fields;
	uint32_t size;

	SmxESField* FindFieldAtOffset( size_t offset )
	{
		for( size_t i = 0; i < num_fields; i++ )
		{
			if( fields[i].offset == offset )
				return &fields[i];
		}
		return nullptr;
	}
};

struct SmxField
{
	const char* name;
	SmxVariableType type;
};

struct SmxClassDef
{
	enum SmxClassDefFlags
	{
		STRUCT = 0
	};

	int flags;
	const char* name;
	size_t num_fields;
	SmxField* fields;
};

class SmxFile
{
public:
	SmxFile( const char* filename );

	SmxFunction* FindFunctionByName( const char* func_name );
	SmxFunction* FindFunctionAt( cell_t addr );
	SmxFunction* FindFunctionById( cell_t id );
	SmxNative*   FindNativeByIndex( size_t index );
	SmxVariable* FindGlobalByName( const char* var_name );
	SmxVariable* FindGlobalAt( cell_t addr );

	size_t num_functions() const { return functions_.size(); }
	SmxFunction& function( size_t index ) { return functions_[index]; }
	size_t num_natives() const { return natives_.size(); }
	SmxNative& native( size_t index ) { return natives_[index]; }
	size_t num_enumerations() const { return enums_.size(); }
	SmxEnum& enumeration( size_t index ) { return enums_[index]; }
	size_t num_type_defs() const { return typedefs_.size(); }
	SmxTypeDef& type_def( size_t index ) { return typedefs_[index]; }
	size_t num_type_sets() const { return typedefs_.size(); }
	SmxTypeDef& type_set( size_t index ) { return typedefs_[index]; }
	size_t num_enum_structs() const { return enum_structs_.size(); }
	SmxEnumStruct& enum_struct( size_t index ) { return enum_structs_[index]; }
	size_t num_globals() const { return globals_.size(); }
	SmxVariable& global( size_t index ) { return globals_[index]; }

	cell_t* code( size_t addr = 0 ) const { return (cell_t*)((uintptr_t)code_ + addr); }
	size_t code_size() const { return code_size_; }
	cell_t* data( size_t addr = 0 ) const { return (cell_t*)((uintptr_t)data_ + addr); }
	size_t data_size() const { return data_size_; }
private:
	SmxSection* GetSectionByName( const char* name );
	void ReadSections();

	void ReadCode( const char* name, size_t offset, size_t size );
	void ReadData( const char* name, size_t offset, size_t size );
	void ReadNames( const char* name, size_t offset, size_t size );
	void ReadPublics( const char* name, size_t offset, size_t size );
	void ReadPubvars( const char* name, size_t offset, size_t size );
	void ReadNatives( const char* name, size_t offset, size_t size );
	void ReadRttiData( const char* name, size_t offset, size_t size );
	void ReadRttiMethods( const char* name, size_t offset, size_t size );
	void ReadRttiNatives( const char* name, size_t offset, size_t size );
	void ReadRttiEnums( const char* name, size_t offset, size_t size );
	void ReadRttiTypeDefs( const char* name, size_t offset, size_t size );
	void ReadRttiTypeSets( const char* name, size_t offset, size_t size );
	void ReadRttiClassdefs( const char* name, size_t offset, size_t size );
	void ReadRttiFields( const char* name, size_t offset, size_t size );
	void ReadRttiEnumStructs( const char* name, size_t offset, size_t size );
	void ReadRttiEnumStructFields( const char* name, size_t offset, size_t size );
	void ReadDbgMethods( const char* name, size_t offset, size_t size );
	void ReadDbgGlobals( const char* name, size_t offset, size_t size );
	void ReadDbgLocals( const char* name, size_t offset, size_t size );

	SmxVariableType DecodeVariableType( uint32_t type_id );
	SmxVariableType DecodeVariableType( unsigned char** data );
	SmxFunctionSignature DecodeFunctionSignature( uint32_t signature );
	SmxFunctionSignature DecodeFunctionSignature( unsigned char** data );
	uint32_t DecodeUint32( unsigned char** data );
private:
	std::unique_ptr<char[]> image_;
	char* stringtab_ = nullptr;
	std::vector<SmxSection> sections_;
	cell_t* code_ = nullptr;
	size_t code_size_ = 0;
	char* data_ = nullptr;
	size_t data_size_ = 0;
	char* names_ = nullptr;
	unsigned char* rtti_data_ = nullptr;
	std::vector<SmxFunction> functions_;
	std::vector<SmxFunction*> rtti_methods_;
	std::vector<SmxNative> natives_;
	std::vector<SmxEnum> enums_;
	std::vector<SmxTypeDef> typedefs_;
	std::vector<SmxTypeSet> typesets_;
	std::vector<SmxESField> es_fields_;
	std::vector<SmxEnumStruct> enum_structs_;
	std::vector<SmxClassDef> classdefs_;
	std::vector<SmxField> fields_;
	std::vector<SmxVariable> globals_;
	std::vector<SmxVariable> locals_;
};