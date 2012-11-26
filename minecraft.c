#include <Python.h>
#include <structmember.h>
#include <regex.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "zlib.h"
#include "minecraft.h"

/*
Just a disclaimer, this is me being extremely rusty at C, so I wouldn't look
to this for coding tips or anything.

Functionality
---------------------

region = load_region(x, z)
chunk = load_chunk(cx, cz)

*/

int CHUNK_DEFLATE_MAX = 65536;
int CHUNK_INFLATE_MAX = 131072;

// Takes a number of bytes (in big-endian order), starting at a given location,
// and returns the integer they represent
long bytes_to_long( unsigned char * buffer, int bytes )
{
    int i, transform;

    transform = 0;
    for ( i = 0; i < bytes; i++ )
    {
        transform = buffer[i]<<((bytes - i - 1) * 8) | transform;
    }
    return transform;
}

// Inflate x bytes of compressed data, using zlib
int inf( unsigned char * dst, unsigned char * src, int bytes )
{
    int ret;
    z_stream strm;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    strm.next_out = dst;
    strm.avail_out = CHUNK_INFLATE_MAX;
    strm.next_in = src;
    strm.avail_in = bytes;

    inflateInit(&strm);
    ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);
    if ( ret != Z_STREAM_END)
    {
        printf("RC: %d | Error: %s\n", ret, strm.msg);
    }

    return ret;
}

// Takes a buffer and prints it prettily
void dump_buffer(unsigned char * buffer, int count)
{
    int i;

    for ( i = 0; i < count; i++)
    {
        printf("%2x", buffer[i]);

        if ( i % 4 == 3 )
            printf(" ");
        if ( i % 16 == 15 )
            printf("\n");
    }
}

// Takes a region file stream and a chunk location and finds and decompresses
// the chunk to the passed buffer
int decompress_chunk( FILE * region_file, unsigned char * chunk_buffer, int chunkX, int chunkZ )
{
    unsigned int offset, chunk_length, compression_type;
    unsigned char small_buffer[5], compressed_chunk_buffer[1048576];

    printf("Finding chunk (%d, %d)\n", chunkX, chunkZ);
    fseek(region_file, 4 * ((chunkX & 31) + (chunkZ & 31) * 32), SEEK_SET);
    fread(small_buffer, 4, 1, region_file);

    offset = bytes_to_long(small_buffer, 3) * 4096;
    if ( offset == 0 )
    {
        //printf("Chunk is empty!\n");
        return 0;
    }

    printf("Offset: %d | Length: %d\n", offset, small_buffer[3] * 4096);

    // Seek to start of chunk, copy it into buffer
    fseek(region_file, offset, SEEK_SET);
    memset(small_buffer, 0, 5);
    fread(small_buffer, 5, 1, region_file);
    chunk_length = bytes_to_long(small_buffer, 4);
    compression_type = bytes_to_long(small_buffer + 4, 1);
    printf("Actual Length: %d\n", chunk_length);
    printf("Compression scheme: %d\n", compression_type);

    fread(compressed_chunk_buffer, chunk_length - 1, 1, region_file);
    inf(chunk_buffer, compressed_chunk_buffer, chunk_length - 1);

    return 1;
}

// Get information from the filename
void region_information( int *coords, char *filename )
{
    regmatch_t matches[3];
    regex_t regex;
    int reti;

    coords[0] = -1;
    coords[1] = -1;

    reti = regcomp(&regex, "^.*\\.([^.]*)\\.([^.]*)\\.mca$", REG_EXTENDED);
    if ( reti )
    {
        printf("Could not compile expression");
        return;
    }

    reti = regexec(&regex, filename, 3, matches, 0);
    if ( !reti )
    {
        char coord[20];

        strncpy ( coord, filename + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_eo + 1 );
        coords[0] = atoi(coord);

        strncpy ( coord, filename + matches[2].rm_so, matches[2].rm_eo - matches[2].rm_eo + 1 );
        coords[1]= atoi(coord);

        printf("\nx: %d | z: %d\n", coords[0], coords[1]);
    } 
    else
    {
        printf("NO MATCH!");
    }
}

/*

Python module set-up

*/
static PyMethodDef MinecraftMethods[] = {
//    {"get_chunk", get_chunk, METH_VARARGS, "Get a specified chunk."},
    {NULL, NULL, 0, NULL} /* Sentinel */
};

PyMODINIT_FUNC initminecraft(void)
{
    PyObject *m;
    
    minecraft_ChunkType.tp_new = PyType_GenericNew;
    if( PyType_Ready(&minecraft_ChunkType) < 0 )
        return;
    
    minecraft_WorldType.tp_new = PyType_GenericNew;
    if( PyType_Ready(&minecraft_WorldType) < 0 )
        return;

    m = Py_InitModule3("minecraft", MinecraftMethods, "Minecraft module");

    Py_INCREF(&minecraft_ChunkType);
    PyModule_AddObject(m, "Chunk", (PyObject *) &minecraft_ChunkType);
    Py_INCREF(&minecraft_WorldType);
    PyModule_AddObject(m, "World", (PyObject *) &minecraft_WorldType);
}

int main( int argc, char *argv[] )
{
    FILE *fp;
    unsigned char chunk[1024000];
    int coords[2];

    Py_Initialize();

    if ( argc < 2 )
    {
        printf("No region filename specified");
        exit(1);
    }

    // Find chunk ranges the file contains
    region_information(coords, argv[1]);

    // Chunk locations
    printf("Opening %s\n---------\n\n", argv[1]);

    if ( fp = fopen(argv[1], "r") )
    {
        decompress_chunk(fp, chunk, 0, 5);
//        chunk_to_dict(chunk);
    }

    fclose(fp);

    return 0;
}
