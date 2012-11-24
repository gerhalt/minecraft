#include <Python.h>
#include <regex.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "zlib.h"

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
int inf( char * dst, char * src, int bytes )
{
    int ret;
    unsigned have;
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
    unsigned int offset, chunk_length, compression_type, rc;
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
    rc = inf(chunk_buffer, compressed_chunk_buffer, chunk_length - 1);

    return 1;
}

// Given a pointer to a payload, return a PyObject representing that payload
// moved will be modified by the amount the tag pointer shifted
PyObject * get_tag( unsigned char * tag, char tag_id, int * moved )
{
    PyObject * payload;

    // The only time the tag_id should be -1 is if the root is passed in
    if( tag_id == -1 )
    {
        printf("ROOT TAG\n");
        tag_id = tag[0];
        tag += 3; // Skip the length of the same, since we know it to be 0
    }

    switch(tag_id)
    {
        long size;
        int i, sub_moved;
        unsigned char list_tag_id;

        case 1: // Byte
            payload = PyInt_FromLong(bytes_to_long(tag, 1));
            *moved += sizeof(char);
            break;

        case 2: // Short
            payload = PyInt_FromLong(bytes_to_long(tag, sizeof(short)));
            *moved += sizeof(short);
            break;

        case 3: // Int
            payload = PyInt_FromLong(bytes_to_long(tag, sizeof(int)));
            *moved += sizeof(int);
            break;

        case 4: // Long
            payload = PyInt_FromLong(bytes_to_long(tag, sizeof(long)));
            *moved += sizeof(long);
            break;

        case 5: // Float
            payload = PyFloat_FromDouble((double) bytes_to_long(tag, sizeof(float)));
            *moved += sizeof(float);
            break;

        case 6: // Double
            payload = PyFloat_FromDouble((double) bytes_to_long(tag, sizeof(double)));
            *moved += sizeof(double);
            break;

        case 7: // Byte array
            size = bytes_to_long(tag, sizeof(int));
            printf("Byte array size: %ld\n", size);
            payload = PyByteArray_FromStringAndSize(tag + sizeof(int), size);
            *moved += sizeof(int) + size;
            break;

        case 11: // Int array
            size = bytes_to_long(tag, sizeof(int));
            tag += sizeof(int);

            payload = PyList_New(size);
            for( i = 0; i < size; i++ )
            {
                PyObject * integer;

                integer = PyInt_FromLong(bytes_to_long(tag, sizeof(int)));
                PyList_SET_ITEM(payload, i, integer);
                tag += sizeof(int);
            }
            *moved += sizeof(int) * (size + 1);
            break;

        case 8: // String
            size = bytes_to_long(tag, sizeof(short));
            payload = PyString_FromStringAndSize(tag + sizeof(short), size);
            *moved += sizeof(short) + size;
            break;

        case 9: // List
            printf("LIST\n");
            list_tag_id = tag[0];
            size = bytes_to_long(tag + 1, sizeof(int));
            tag += 1 + sizeof(int);

            payload = PyList_New(size);
            sub_moved = 0;
            for( i = 0; i < size; i++ )
            {
                PyObject * list_item;

                list_item = get_tag(tag, list_tag_id, &sub_moved);
                PyList_SET_ITEM(payload, i, list_item);
                tag += sub_moved;
                printf("list item moved %d bytes", sub_moved);
            }
            *moved += 5 + sub_moved;
            break;

        case 10: // Compound
            payload = PyDict_New();
            printf("--> COMPOUND TAG, GOING DEEPER!\n");

            do // Repeatedly grab tags until an end tag is seen 
            {
                printf("GRABBING TAG\n");
                PyObject * sub_payload;
                unsigned char sub_tag_id;
                unsigned char sub_tag_name[1000];
                int sub_tag_name_length;

                sub_tag_id = *tag;
                if( sub_tag_id == 0 )
                    break; // Found the end of our compound tag

                sub_tag_name_length = bytes_to_long(tag + 1, 2);;

                unsigned char * name_buffer[sub_tag_name_length];
                strncpy(sub_tag_name, tag + 3, sub_tag_name_length);
                if( sub_tag_name_length == 0 )
                {
                    printf("Something is wrong, label missing");
                    return NULL;
                }

                printf("Tag ID: %d | Name: %.*s\n", sub_tag_id, sub_tag_name_length, sub_tag_name);

                tag += 3 + sub_tag_name_length;
                sub_moved = 0;
                sub_payload = get_tag(tag, sub_tag_id, &sub_moved);
                PyDict_SetItemString(payload, sub_tag_name, sub_payload);

                printf("%d bytes\n", sub_moved);
                tag += sub_moved;
                
                *moved += 3 + sub_tag_name_length + sub_moved;
            }
            while( true );

            printf("--> END OF COMPOUND TAG");
            break;           

        default:
            printf("UNCAUGHT TAG ID: %d\n", tag_id);
            payload = NULL;
            break;
    }
    return payload;
}

// Wrapper for get_tag(tag, tag_id)
PyObject * get_chunk_dict( unsigned char * root )
{
    int moved;

    moved = 0;
    return get_tag(root, -1, &moved);
}

// Convert a chunk to python dictionary format
void chunk_to_dict( unsigned char * chunk_buffer )
{
    /*
    root {
          'Level': {
                    'xPos': 1, 'zPos': 2, ...
    */
    PyObject * dict;

    dict = get_chunk_dict(chunk_buffer);
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

Python Module

*/

static PyObject * get_chunk(PyObject *self, PyObject *args)
{
    


}

// Method table
static PyMethodDef MinecraftMethods[] = {
    {"get_chunk", get_chunk, METH_VARARGS, "Get a specified chunk."},
    {NULL, NULL, 0, NULL} /* Sentinel */
};

// Module initialization
PyMODINIT_FUNC initminecraft(void)
{
    (void) Py_InitModule("minecraft", MinecraftMethods);
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
        chunk_to_dict(chunk);
    }

    fclose(fp);
}

