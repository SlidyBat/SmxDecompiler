#pragma once

#include <vector>
#include <memory>

using cell_t = int32_t;

struct SmxFunctionSignature
{
	int nargs;
	bool varargs;
};

struct SmxFunction
{
	const char* name;
	cell_t* pcode_start;
	cell_t* pcode_end;
	SmxFunctionSignature signature;
};

struct SmxNative
{
	const char* name;
	SmxFunctionSignature signature;
};

class SmxFile
{
public:
	SmxFile( const char* filename );

	size_t num_functions() const { return functions_.size(); }
	SmxFunction& function( size_t index ) { return functions_[index]; }
	size_t num_natives() const { return natives_.size(); }
	SmxNative& native( size_t index ) { return natives_[index]; }

	cell_t* code() { return code_; }
	char* data() { return data_; }
private:
	void ReadSection( const char* name, size_t offset, size_t size );

	void ReadCode( const char* name, size_t offset, size_t size );
	void ReadData( const char* name, size_t offset, size_t size );
	void ReadNames( const char* name, size_t offset, size_t size );
	void ReadRttiData( const char* name, size_t offset, size_t size );
	void ReadRttiMethods( const char* name, size_t offset, size_t size );
	void ReadRttiNatives( const char* name, size_t offset, size_t size );
private:
	std::unique_ptr<char[]> image_;
	char* stringtab_ = nullptr;
	cell_t* code_ = nullptr;
	char* data_ = nullptr;
	char* names_ = nullptr;
	char* rtti_data_ = nullptr;
	std::vector<SmxFunction> functions_;
	std::vector<SmxNative> natives_;
};