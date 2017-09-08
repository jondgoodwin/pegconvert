# Pegasus3D Converter
Command-line tool for converting 3D content from other formats to Acorn programs.

Specify two parameters: the path and filename of the source file and the path and filename
of the target Acorn program. Use '?' to see the available options for converting coordinates.

Currently, the converter only supports 3-D mesh conversion from Lightwave .obj,
resulting in an Acorn program that returns a Shape. Material information is ignored.

## Building (Linux)

To build the library:

	make

## Building (Windows)

A Visual C++ 2010 solutions file can be created using the project files. 
The generated object, library, and executable files are created relative to the location of the 
solutions file.

## Documentation

Use [Doxygen][] to generate [documentation][doc] from the source code.

## License

Copyright (C) 2017  Jonathan Goodwin

The Pegasus3D Converter (pegconvert) is distributed under the terms of the MIT license. 
See LICENSE for details.