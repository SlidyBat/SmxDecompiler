# SmxDecompiler

A decompiler for .smx files.

### Disclaimer
This project is for learning purposes only, and therefore won't be actively supported or developed. The code is very leaky, unstable and slow. Invalid inputs will likely cause it to crash.

## Usage
```
SmxDecompiler [--function/-f <function>] [--strings <none/aggressive/comment] [--no-globals/-g] [--assembly/-a] [--il/-i] <filename>

 --function    -f      Only decompiles the specified function
 --strings     -s      Sets how decompiler should try to detect strings:
                         `none` - Does not try to detect strings at all (default)
                         `aggressive` - Replaces any number that could be a string into a string
                         `comment` - Adds a comment next to a number indicating what string it could be
 --no-globals  -g      Does not print the globals section
 --assembly    -a      Prints the disassembly for each function along with its code
 --il          -i      Prints the lited IL for each function along with its code
```
