#pragma once

#include <vector>
#include <string>
#include <unordered_map>

class OptParse
{
public:
	// Adds an option with an optional argument to parse
	// Default value is only used when the user does not specify an argument for the option
	OptParse& AddArgOption( const std::string& longname, const std::string& defaultval = "" )
	{
		AddArgOption( longname, NO_SHORTNAME, defaultval );
		return *this;
	}

	// Adds an option with an optional argument to parse with a shortname
	// Default value is only used when the user does not specify an argument for the option
	OptParse& AddArgOption( const std::string& longname, char shortname, const std::string& defaultval = "" )
	{
		options.push_back( { longname, shortname, defaultval, true } );
		return *this;
	}

	// Adds an option that is used as a flag (ie. no arguments should be parsed for it)
	OptParse& AddFlagOption( const std::string& longname, char shortname = NO_SHORTNAME )
	{
		options.push_back( { longname, shortname, "", false } );
		return *this;
	}

	void Process( int argc, const char** argv )
	{
		for( int i = 1; i < argc; i++ )
		{
			if( argv[i][0] == '-' && argv[i][1] == '-' && argv[i][2] != ' ' )
			{
				i = ProcessLongOption( i, argc, argv );
			}
			else if( argv[i][0] == '-' && argv[i][1] != ' ' )
			{
				i = ProcessShortOption( i, argc, argv );
			}
			else
			{
				AddStrayString( argv[i] );
			}
		}
	}

	// Gets number of args that aren't an argument to any options
	size_t GetArgC() const { return stray_strings.size(); }
	// Gets argument at specified index (args that aren't an argument to any options)
	std::string GetArg( int index ) const { return stray_strings[index]; };

	// Returns whether the specified option exists (was 
	bool Exists( const std::string& opt ) const
	{
		auto it = option_args.find( opt );
		if( it == option_args.end() )
		{
			return false;
		}

		return true;
	}
	// Returns the argument for the option if it exists, or nullptr if it doesn't
	const char* Get( const std::string& opt ) const
	{
		auto it = option_args.find( opt );
		if( it == option_args.end() )
		{
			return nullptr;
		}

		return it->second.c_str();
	}
	// Returns the argument for the option if it exists, or nullptr if it doesn't
	const char* operator[]( const std::string& opt ) const
	{
		return Get( opt );
	}
private:
	int ProcessLongOption( int index, int argc, const char** argv )
	{
		std::string option = &argv[index][2]; // get the option without the -- at the start
		for( const Option& o : options )
		{
			if( o.longname == option )
			{
				if( index + 1 < argc &&
					o.has_argument &&
					argv[index + 1][0] != '-' )
				{
					option_args[option] = argv[++index];
				}
				else
				{
					option_args[option] = o.defaultval;
				}

				break;
			}
		}

		return index;
	}
	int ProcessShortOption( int index, int argc, const char** argv )
	{
		std::string shortoptions = &argv[index][1];
		for( size_t i = 0; i < shortoptions.size(); i++ )
		{
			for( const Option& o : options )
			{
				if( o.shortname == shortoptions[i] )
				{
					// No args allowed for multiple short args
					// e.g. -abcd
					if( shortoptions.size() == 1 &&
						o.has_argument &&
						index + 1 < argc &&
						argv[index + 1][0] != '-' )
					{
						option_args[o.longname] = argv[++index];
					}
					else
					{
						option_args[o.longname] = o.defaultval;
					}

					break;
				}
			}
		}

		return index;
	}

	void AddStrayString( const std::string& stray )
	{
		stray_strings.push_back( stray );
	}
private:
	static constexpr char NO_SHORTNAME = 0;

	struct Option
	{
		std::string longname;
		char shortname;
		std::string defaultval;
		bool has_argument;
	};

	std::vector<Option> options;
	std::vector<std::string> stray_strings;
	std::unordered_map<std::string, std::string> option_args;
};
