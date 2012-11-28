/*
ntb.c

Decompression and NBT reading utilities
*/

#include <Python.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "minecraft.h"
#include "zlib.h"

// Takes a buffer and prints it prettily
void dump_buffer(unsigned char * buffer, int count)
{
    int i;

    for ( i = 0; i < count; i++)
    {
        if ( i % 16 == 0 )
            printf(" %4i ", i); 

        printf("%2x", buffer[i]);

        if ( i % 4 == 3 )
            printf(" ");
        if ( i % 16 == 15 )
            printf("\n");
    }
    printf("\n");
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

    inflateInit2(&strm, MAX_WBITS + 32); // + 32 bits for header detection and gzip
    ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);
    
    if ( ret != Z_STREAM_END )
        PyErr_Format(PyExc_Exception, "Unable to decompress (RC: %d | Error: %s)", ret, strm.msg);

    return ret;
}

// Deflate, using the same idea as the "inflate" function
int def( unsigned char * dst, unsigned char * src, int bytes )
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

    deflateInit2(&strm, 
                 Z_DEFAULT_COMPRESSION,
                 Z_DEFLATED, 
                 MAX_WBITS + 16,
                 8,
                 Z_DEFAULT_STRATEGY); // + 16 bits for simple gzip header
    ret = deflate(&strm, Z_FINISH);
    deflateEnd(&strm);
    
    if ( ret != Z_STREAM_END )
        PyErr_Format(PyExc_Exception, "Unable to compress (RC: %d | Error: %s)", ret, strm.msg);

    return ret;
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

// Given a pointer to a payload, return a PyObject representing that payload
// moved will be modified by the amount the tag pointer shifted
PyObject * get_tag( unsigned char * tag, char tag_id, int * moved )
{
    PyObject * payload;

    // The only time the tag_id should be -1 is if the root is passed in
    if( tag_id == -1 )
    {
        tag_id = tag[0];
        tag += 3; // Skip the length of the tag, since we know it to be 0
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
            tag += sizeof(int);

            payload = PyByteArray_FromStringAndSize((char *) tag, size);
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
            tag += sizeof(short);

            payload = PyString_FromStringAndSize((char *) tag, size);

            // Take care of the special case where a boolean value is 
            // represented as a string, and convert to a Python boolean object
            if( strcmp(PyString_AsString(payload), "true") )
                payload = Py_True;
            else if( strcmp(PyString_AsString(payload), "false") )
                payload = Py_False;

            *moved += sizeof(short) + size;
            break;

        case 9: // List
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
            }
            *moved += 5 + sub_moved;
            break;

        case 10: // Compound
            payload = PyDict_New();
            do // Repeatedly grab tags until an end tag is seen 
            {
                PyObject * sub_payload;
                unsigned char sub_tag_id;
                char * sub_tag_name;
                int sub_tag_name_length;

                sub_tag_id = *tag;
                if( sub_tag_id == 0 )
                    break; // Found the end of our compound tag

                sub_tag_name_length = bytes_to_long(tag + 1, 2);;

                // This should never happen, but the check doesn't hurt
                if( sub_tag_name_length == 0 )
                    return NULL; 

                sub_tag_name = calloc(sub_tag_name_length + 1, 1);
                strncpy(sub_tag_name, (char *) tag + 3, sub_tag_name_length);
                // printf("Tag ID: %d | Name: %.*s\n", sub_tag_id, sub_tag_name_length, sub_tag_name);

                tag += 3 + sub_tag_name_length;
                sub_moved = 0;
                sub_payload = get_tag(tag, sub_tag_id, &sub_moved);
                PyDict_SetItemString(payload, sub_tag_name, sub_payload);

                tag += sub_moved;
                *moved += 3 + sub_tag_name_length + sub_moved;

                free(sub_tag_name);
            }
            while( true );
            break;           

        default:
            printf("UNCAUGHT TAG ID: %d\n", tag_id);
            return NULL;
    }

    Py_INCREF(payload);
    return payload;
}

// For testing
int main( int argc, char *argv[] )
{
    FILE *fp;
    unsigned char * src, * dst;

    if ( argc < 2 )
    {
        printf("No region filename specified");
        exit(1);
    }

    /*

    TEST CODE

    */

    src = calloc(10000, 1);
    dst = calloc(10000, 1);

    fp = fopen(argv[1], "rb"); // TODO: Does 'rb' vs 'r' mean anything?
    if( fp != NULL )
    {
        struct stat stbuf;
        int size;

        fstat(fileno(fp), &stbuf);
        size = stbuf.st_size;
        printf("Size is: %d\n", size);

        fread(src, 1, size, fp);
        dump_buffer(src, 600);

        inf(dst, src, size);

        dump_buffer(dst, 600);
    }

    free(src);
    free(dst);

    /*

    END OF TEST CODE

    */

    fclose(fp);

    return 0;
}
