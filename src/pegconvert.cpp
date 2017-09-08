/** Pegasus3d Converter - main program
 * @file
 *
 * This source file is part of the Pegasus3d Converter.
 * Copyright (C) 2017  Jonathan Goodwin
 *
 * Permission is hereby granted, free of charge, to any
 * person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the
 * Software without restriction, including without
 * limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software
 * is furnished to do so, subject to the following
 * conditions:

 * The above copyright notice and this permission notice
 * shall be included in all copies or substantial portions
 * of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 * ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
 * SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// Parameters affecting transformation of vertex positions as needed for converted mesh
float yrotation = 0.0;	// How much to rotate converted mesh around y-axis clockwise as looking down from on top
float xorigin = -1.0;	// Where to translate x coordinates. <0 unchanged. 0 at bottom, 0.5 in center, 1 at top
float yorigin = -1.0;	// Where to translate y coordinates. <0 unchanged. 0 at bottom, 0.5 in center, 1 at top
float zorigin = -1.0;	// Where to translate z coordinates. <0 unchanged. 0 at bottom, 0.5 in center, 1 at top
float height = -1.0;	// Total height of converted mesh. <0 unchanged.
float xdelta = 0.0;		// Calculated change to x before scaling
float ydelta = 0.0;		// Calculated change to y before scaling
float zdelta = 0.0;		// Calculated change to z before scaling
float scale = 1.0;		// Calculated scaling factor
int uvdim = 2;			// Number of uv dimensions to generate (2/3)

/** Information about contents of a loaded source file */
struct FileInfo {
	char *contents;		//!< The allocated area containing file's contents
	size_t size;		//!< The size of the allocated area
	char *ptr;			//!< A working pointer that navigates the contents
};

int objnvertpos = 0;	//!< Number of vertex positions in the .obj file
int objnvertnorm = 0;	//!< Number of vertex normals in the .obj file
int objnverttex = 0;	//!< Number of vertex textures in the .obj file
int objnfaces = 0;		//!< Number of faces in the .obj file
int objnfaceidxs = 0;	//!< Number of vertex references by faces in the .obj file

/** Load the file into an allocated block. Return 0 if not found */
int loadfile(struct FileInfo *file, const char *fn) {
	// Open the file - return null on failure
	FILE *fd;
	if (!(fd = fopen(fn, "rb"))) {
		printf("Cannot open file: %s", fn);
		return 0;
	}

	// Determine the file length
	fseek(fd, 0, SEEK_END);
	file->size=ftell(fd);
	fseek(fd, 0, SEEK_SET);

	// Create the string buffer (which will be returned)
	file->ptr = file->contents = (char *) malloc(file->size+1);

	// Load the data into an allocated buffer
	fread((void *)file->contents, 1, file->size, fd);
	file->contents[file->size]='\0';

	// Close the file
	fclose(fd);
	return 1;
}

/** Free the allocated file block */
void closefile(struct FileInfo *file) {
	free(file->contents);
}

/** Match a string to file's contents, after skipping spaces.
	Return 1 if found, and skip past. Return 0 otherwise. */
int match(FileInfo* obj, const char *str) {
	while (*obj->ptr && *obj->ptr==' ')
		obj->ptr++;
	if (0==strncmp(obj->ptr, str, strlen(str))) {
		obj->ptr += strlen(str);
		return 1;
	} else
		return 0;
}

/** Move file point to start of next line */
void nextline(FileInfo* obj) {
	while (*obj->ptr && *obj->ptr!='\n')
		obj->ptr++;
	if (*obj->ptr)
		obj->ptr++;
}

/** Return a floating point number from source, ignoring leading spaces.
	Move pointer past the number */
float getfloat(FileInfo* obj) {
	float nbr;
	while (*obj->ptr && *obj->ptr==' ')
		obj->ptr++;
	nbr = (float) atof(obj->ptr);
	while (*obj->ptr && ((*obj->ptr>='0' && *obj->ptr<='9') || *obj->ptr=='.' || *obj->ptr=='-' || toupper(*obj->ptr)=='E'))
		obj->ptr++;
	return nbr;
}

/** Return an integer from source, ignoring leading spaces.
	Move pointer past the number */
int getint(FileInfo* obj) {
	int nbr;
	while (*obj->ptr && *obj->ptr==' ')
		obj->ptr++;
	nbr = atoi(obj->ptr);
	while (*obj->ptr && ((*obj->ptr>='0' && *obj->ptr<='9')))
		obj->ptr++;
	return nbr;
}

/** From .obj, one vertex defined by a face's polygon */
struct objfaceidxstr {
	int seggroup;	//!< Current smoothing segment group face is part of
	int vp;			//!< Index for vertex position
	int vt;			//!< Index for vertex texture (could be 0)
	int vn;			//!< Index for vertex normal (could be 0)
};

// Retrieved 3D model information from .obj file
float *objvertpos;		//!< Buffer holding all .obj vertex x,y,z positions
float *objvertposp;		//!< Pointer into .obj vertex position buffer
float *objvertnorm;		//!< Buffer holding all .obj vertex normals
float *objvertnormp;	//!< Pointer into .obj vertex normal buffer
float *objverttex;		//!< Buffer holding all .obj vertex uvw texture coordinates
float *objverttexp;		//!< Pointer into .obj vertex texture buffer
struct objfaceidxstr *objfaceidx;	//!< Buffer holding all .obj vertex indices for faces
struct objfaceidxstr *objfaceidxp;	//!< Working pointer into objfaceidx
int *objfacepos;		//!< Buffer holding starting positions in objfaceidx for vertex indexes
int *objfaceposp;		//!< Working pointer into objfacepos
int *objfacelen;		//!< Buffer holding number of vertices for every face's polygon
int *objfacelenp;		//!< Working pointer into objfacelen
int seggroup;			//!< Current smoothing segment number while scanning .obj

float maxx=0;
float minx=0;
float maxy=0;
float miny=0;
float maxz=0;
float minz=0;

/** Capture all vertex and face information from .obj file.
	If cnt is true, only obtain counts. Otherwise, fill the obj buffers. */
int parseobj(FileInfo* obj, bool cnt) {
	if (!cnt) {
		objvertposp = objvertpos = (float *) malloc(3*objnvertpos*sizeof(float));
		objvertnormp = objvertnorm = (float *) malloc(3*objnvertnorm*sizeof(float));
		objverttexp = objverttex = (float *) malloc(3*objnverttex*sizeof(float));
		objfaceposp = objfacepos = (int *) malloc(objnfaces*sizeof(int));
		objfacelenp = objfacelen = (int *) malloc(objnfaces*sizeof(int));
		objfaceidxp = objfaceidx = (struct objfaceidxstr *) malloc(objnfaceidxs*sizeof(struct objfaceidxstr));
	}

	// Scan each line
	int faceidxpos = 0;
	while (*obj->ptr) {
		// Ignore leading spaces
		while (*obj->ptr && *obj->ptr==' ')
			obj->ptr++;

		// Process based on line's command
		if (match(obj, "#"))
			nextline(obj);
		else if (match(obj, "vn")) {
			if (cnt) objnvertnorm++;
			else {
				*objvertnormp++ = getfloat(obj);
				*objvertnormp++ = getfloat(obj);
				*objvertnormp++ = getfloat(obj);
			}
			nextline(obj);
		} else if (match(obj, "vt")) {
			if (cnt) objnverttex++;
			else {
				*objverttexp++ = getfloat(obj);
				*objverttexp++ = getfloat(obj);
				*objverttexp++ = getfloat(obj);
			}
			nextline(obj);
		} else if (match(obj, "vp")) {
			nextline(obj);
		} else if (match(obj, "v")) {
			if (cnt) objnvertpos++;
			else {
				float x,y,z;
				*objvertposp++ = x = getfloat(obj);
				*objvertposp++ = y = getfloat(obj);
				*objvertposp++ = z = getfloat(obj);
				// Capture minimums and maximums
				if (x>maxx) maxx=x;
				if (x<minx) minx=x;
				if (y>maxy) maxy=y;
				if (y<miny) miny=y;
				if (z>maxz) maxz=z;
				if (z<minz) minz=z;
			}
			nextline(obj);
		} else if (match(obj, "f")) {
			int vpi=0;
			int vti=0;
			int vni=0;
			int idxcnt=0;
			if (cnt) objnfaces++;
			else *objfaceposp++ = faceidxpos;
			do {
				idxcnt++;
				vpi=getint(obj);
				if (*obj->ptr == '/') {
					obj->ptr++;
					if (*obj->ptr!='/')
						vti = getint(obj);
					if (*obj->ptr == '/') {
						obj->ptr++;
						vni = getint(obj);
					}
				}
				while (*obj->ptr && *obj->ptr==' ')
					obj->ptr++;
				if (cnt) objnfaceidxs++;
				else {
					objfaceidxp->seggroup = seggroup;
					objfaceidxp->vn = vni;
					objfaceidxp->vp = vpi;
					objfaceidxp->vt = vti;
					objfaceidxp++;
					faceidxpos++;
				}
			} while (*obj->ptr && *obj->ptr!='\r' && *obj->ptr!='\n');
			if (!cnt)
				*objfacelenp++ = idxcnt;
			nextline(obj);
		} else if (match(obj, "s")) {
			seggroup = getint(obj);
			nextline(obj);
		} else if (match(obj, "g")) {
			nextline(obj);
		} else if (match(obj, "mtllib")) {
			nextline(obj);
		} else if (match(obj, "usemtl")) {
			nextline(obj);
		} else if (match(obj, "\r") || match(obj, "\n")) {
			nextline(obj);
		} else if (*obj->ptr!='\0') {
			obj->ptr[20] = '\0';
			printf("Unknown line with command %s\n", obj->ptr);
			return 0;
		}
	}
	obj->ptr = obj->contents; // reset for next scan
	if (cnt) {
		printf("Number of vertex positions: %d, normals: %d, uvs: %d\n", objnvertpos, objnvertnorm, objnverttex);
		printf("Number of faces: %d, indices: %d\n", objnfaces, objnfaceidxs);
	}
	else {
		printf("Mins (%f, %f, %f) to Maxs (%f, %f, %f)\n", minx, miny, minz, maxx, maxy, maxz);
		if (height>0.0)
			scale = height/(maxy-miny);
		if (xorigin>=0.0)
			xdelta = -(xorigin*(maxx-minx)+minx);
		if (yorigin>=0.0)
			ydelta = -(yorigin*(maxy-miny)+miny);
		if (zorigin>=0.0)
			zdelta = -(zorigin*(maxz-minz)+minz);
	}
	return 1;
}

int nmaps;		//!< Number of opengl vertexes registered in mapidx
struct objfaceidxstr *mapidx;	//!< Maps a single opengl vertex index to corresponding .obj index trio

/** Map a .obj trio vertex index used by an .obj face to a single opengl vertex index.
	All references to the opengl vertex must share the same segment group and three .obj index numbers. */
int mapindex(struct objfaceidxstr *objindex) {
	// Awful performance, due to sequential search
	for (int i=0; i<nmaps; i++) {
		if ((&mapidx[i])->seggroup == objindex->seggroup
			&& (&mapidx[i])->vn == objindex->vn
			&& (&mapidx[i])->vt == objindex->vt
			&& (&mapidx[i])->vp == objindex->vp)
			return i;
	}
	// Create a new opengl index, if not already found
	(&mapidx[nmaps])->seggroup = objindex->seggroup;
	(&mapidx[nmaps])->vn = objindex->vn;
	(&mapidx[nmaps])->vt = objindex->vt;
	(&mapidx[nmaps])->vp = objindex->vp;
	return nmaps++;
}

struct Xyz {
	float x;
	float y;
	float z;
};

/** Convert original file's xyz position to requested local space */
void convertPosition(struct Xyz *newxyz, struct Xyz *oldxyz) {
	newxyz->x = (oldxyz->x+xdelta)*scale*cos(yrotation) - (oldxyz->z+zdelta)*scale*sin(yrotation);
	newxyz->y = (oldxyz->y+ydelta)*scale;
	newxyz->z = (oldxyz->z+zdelta)*scale*cos(yrotation) + (oldxyz->x+xdelta)*scale*sin(yrotation);
}

/** Convert original file's normals to requested local space */
void convertNormal(struct Xyz *newxyz, struct Xyz *oldxyz) {
	newxyz->x = oldxyz->x*cos(yrotation) - oldxyz->z*sin(yrotation);
	newxyz->y = oldxyz->y;
	newxyz->z = oldxyz->z*cos(yrotation) + oldxyz->x*sin(yrotation);
}

/** Generate an Acorn Shape program from .obj mesh info */
void genobj2acorn(char *fn) {
	FILE *acn = fopen(fn, "w");
	char *lastcomma = ",";

	fprintf(acn, "# pegconvert conversion from .obj to .acn\n");
	struct Xyz oldmin, newmin, oldmax, newmax;
	oldmin.x = minx; oldmin.y = miny; oldmin.z = minz;
	oldmax.x = maxx; oldmax.y = maxy; oldmax.z = maxz;
	convertPosition(&newmin, &oldmin); convertPosition(&newmax, &oldmax);
	fprintf(acn, "+Shape\n\t# boundary: +BoundaryBox(+Xyz(%f, %f, %f), +Xyz(%f, %f, %f))\n",
		newmin.x, newmin.y, newmin.z, newmax.x, newmax.y, newmax.z);

	// Build vertices index first
	fputs("\tindices: +Integers\"\n", acn);
	mapidx = (struct objfaceidxstr*) malloc(objnfaceidxs*sizeof(struct objfaceidxstr));
	nmaps = 0;
	int ntris = 0;
	for (int faceidx=0; faceidx<objnfaces; faceidx++) {
		struct objfaceidxstr *faceidxp = &objfaceidx[objfacepos[faceidx]];
		int v0 = mapindex(&faceidxp[0]);
		int facelen = objfacelen[faceidx];
		for (int i=0; i<facelen-2; i++) {
			if (faceidx==objnfaces-1 && i==facelen-3)
				lastcomma = "\"";
			int v1 = mapindex(&faceidxp[1]);
			int v2 = mapindex(&faceidxp[2]);
			if ((i&1)==0)
				fprintf(acn, "\t\t%d,%d,%d%s\n", v0, v1, v2, lastcomma);
			else
				fprintf(acn, "\t\t%d,%d,%d%s\n", v0, v1, v2, lastcomma);
			ntris++;
			faceidxp++;
		}
	}
	printf("Number of generated vertices: %d and triangles: %d\n", nmaps, ntris);
	fprintf(acn, "# Number of generated vertices: %d and triangles: %d\n", nmaps, ntris);

	// Build position vertices
	fputs("\tpositions: +Xyzs\"\n", acn);
	lastcomma = ",";
	for (int i=0; i<nmaps; i++) {
		if (i==nmaps-1) lastcomma="\"";
		float *f = &objvertpos[3*((&mapidx[i])->vp-1)];
		struct Xyz oxyz, nxyz;
		oxyz.x = f[0]; oxyz.y=f[1]; oxyz.z=f[2];
		convertPosition(&nxyz, &oxyz);
		fprintf(acn, "\t\t%f,%f,%f%s\n", nxyz.x, nxyz.y, nxyz.z, lastcomma);
	}

	// Build "normal" vertices
	if (objnvertnorm>0) {
		fputs("\tnormals: +Xyzs\"\n", acn);
		lastcomma = ",";
		for (int i=0; i<nmaps; i++) {
			if (i==nmaps-1) lastcomma="\"";
			float *f = &objvertnorm[3*((&mapidx[i])->vn-1)];
			struct Xyz oxyz, nxyz;
			oxyz.x = f[0]; oxyz.y=f[1]; oxyz.z=f[2];
			convertNormal(&nxyz, &oxyz);
			fprintf(acn, "\t\t%f,%f,%f%s\n", nxyz.x, nxyz.y, nxyz.z, lastcomma);
		}
	}

	// Build "uv" vertices
	if (objnverttex>0) {
		fputs(uvdim==3? "\tuvs: +Xyzs\"\n" : "\tuvs: +Uvs\"\n", acn);
		lastcomma = ",";
		for (int i=0; i<nmaps; i++) {
			if (i==nmaps-1) lastcomma="\"";
			float *f = &objverttex[3*((&mapidx[i])->vt-1)];
			if (uvdim==3)
				fprintf(acn, "\t\t%f,%f,%f%s\n", f[0], f[1], f[2], lastcomma);
			else
				fprintf(acn, "\t\t%f,%f%s\n", f[0], f[1], lastcomma);
		}
	}

	fclose(acn);
	puts("Conversion successfully completed.\n");
}

/** Convert a file (e.g., .obj) to an Acorn program.
	Accepts two parameters: source file and target filename. */
int main(int argc, char* argv[])
{
	struct FileInfo infile; //!< Contents of the file to be converted
	if (argc<3 || 0==strncmp(argv[1],"?",1)) {
		puts("Please specify the name of the file to convert and the target file.");
		puts("Available options after both file names:");
		puts("  -rotate:##       degrees clockwise to rotate model looking down from top");
		puts("  -origin:bottom   translate model so that 0,0,0 is set to center x,z and bottom y");
		puts("  -origin:center   translate model so that 0,0,0 is set to center x,y,z");
		puts("  -height:###      scale evenly so that model's total height (#) is as specified");
		puts("  -uv:###          How many uv values (2 or 3)");
		return 1;
	}

	if (!loadfile(&infile, argv[1])) {
		printf("Cannot open source file %s\n", argv[1]);
		return 1;
	}

	// Process argument options
	yrotation = 0.0;
	yorigin = -1.0;
	height = -1.0;
	uvdim = 2;
	for (int i=3; i<argc; i++) {
		struct FileInfo arg;
		arg.ptr = argv[i];
		if (match(&arg, "-rotate:"))
			yrotation = (float)M_PI * getfloat(&arg)/180.f;
		else if (match(&arg, "-origin:bottom"))
			{yorigin=0.0; xorigin=0.5; zorigin=0.5;}
		else if (match(&arg, "-origin:center"))
			{xorigin=0.5; yorigin=0.5; zorigin=0.5;}
		else if (match(&arg, "-height:"))
			height = getfloat(&arg);
		else if (match(&arg, "-uv:"))
			uvdim = getint(&arg);
	}

	// Scan for count, scan to get data, then generate
	parseobj(&infile, true);
	parseobj(&infile, false);
	genobj2acorn(argv[2]);

	// Clean up
	free(objvertpos);
	free(objvertnorm);
	free(objfacepos);
	free(objfacelen);
	free(objfaceidx);
	closefile(&infile);
	getchar();
	return 0;
}

