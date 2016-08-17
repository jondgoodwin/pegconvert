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

Copyright (C) 2016  Jonathan Goodwin

 This program is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
