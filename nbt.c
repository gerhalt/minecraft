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

typedef struct TagType{
    char * name;
    int id;
} TagType;

// TODO: This may not be the best place to put this
#define TAG_END         0
#define TAG_BYTE        1
#define TAG_SHORT       2
#define TAG_INT         3
#define TAG_LONG        4
#define TAG_FLOAT       5
#define TAG_DOUBLE      6
#define TAG_BYTE_ARRAY  7
#define TAG_STRING      8
#define TAG_LIST        9
#define TAG_COMPOUND    10
#define TAG_INT_ARRAY   11

static TagType leveldat_tags[] = {
    {"Data", TAG_COMPOUND},
    {"version", TAG_INT},
    {"initialized", TAG_BYTE},
    {"LevelName", TAG_STRING},
    {"generatorName", TAG_STRING},
    {"generatorVersion", TAG_INT},
    {"generatorOptions", TAG_STRING},
    {"RandomSeed", TAG_LONG},
    {"MapFeatures", TAG_BYTE},
    {"LastPlayed", TAG_LONG},
    {"SizeOnDisk", TAG_LONG},
    {"allowCommands", TAG_BYTE},
    {"hardcore", TAG_BYTE},
    {"GameType", TAG_INT},
    {"Time", TAG_LONG},
    {"DayTime", TAG_LONG},
    {"SpawnX", TAG_INT},
    {"SpawnY", TAG_INT},
    {"SpawnZ", TAG_INT},
    {"raining", TAG_BYTE},
    {"rainTime", TAG_INT},
    {"thundering", TAG_BYTE},
    {"thunderTime", TAG_INT},
    {"Player", TAG_COMPOUND},
    {"GameRules", TAG_COMPOUND},
    {"commandBlockOutput", TAG_STRING},
    {"doFireTick", TAG_STRING},
    {"doMobLoot", TAG_STRING},
    {"doMobSpawning", TAG_STRING},
    {"doTileDrops", TAG_STRING},
    {"keepInventory", TAG_STRING},
    {"mobGriefing", TAG_STRING},
    {NULL, NULL} // Sentinel
};

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

/*
Inflate
  * dst - destination
  * src - source
  bytes - number of bytes in the inflation buffer
  mode  - compression mode to read
    0   - normal (zlib)
    1   - gzip (including headers)
*/
int inf( unsigned char * dst, unsigned char * src, int bytes, int mode )
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

    inflateInit2(&strm, MAX_WBITS + mode * 32); // + 32 bits for header detection and gzip
    ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);
    
    if ( ret != Z_STREAM_END )
        PyErr_Format(PyExc_Exception, "Unable to decompress (RC: %d | Error: %s)", ret, strm.msg);

    return ret;
}

/*
Deflate
  * dst - destination
  * src - source
  bytes - number of bytes in the deflate buffer
  mode  - compression mode to use
    0   - normal (zlib)
    1   - gzip (including headers)
*/
int def( unsigned char * dst, unsigned char * src, int bytes, int mode )
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
                 MAX_WBITS + mode * 16,  // + 16 bits for simple gzip header
                 8,
                 Z_DEFAULT_STRATEGY);
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
    inf(chunk_buffer, compressed_chunk_buffer, chunk_length - 1, 0);

    return 1;
}

// Given a pointer to a payload, return a PyObject representing that payload
// moved will be modified by the amount the tag pointer shifted
PyObject * get_tag( unsigned char * tag, char id, int * moved )
{
    PyObject * payload;

    // The only time the id should be -1 is if the root is passed in
    if( id == -1 )
    {
        id = tag[0];
        tag += 3; // Skip the length of the tag, since we know it to be 0
    }

    switch(id)
    {
        long size;
        int i, sub_moved;
        unsigned char list_id;

        case TAG_BYTE: // Byte
            payload = PyInt_FromLong(bytes_to_long(tag, 1));
            *moved += sizeof(char);
            break;

        case TAG_SHORT: // Short
            payload = PyInt_FromLong(bytes_to_long(tag, sizeof(short)));
            *moved += sizeof(short);
            break;

        case TAG_INT: // Int
            payload = PyInt_FromLong(bytes_to_long(tag, sizeof(int)));
            *moved += sizeof(int);
            break;

        case TAG_LONG: // Long
            payload = PyInt_FromLong(bytes_to_long(tag, sizeof(long)));
            *moved += sizeof(long);
            break;

        case TAG_FLOAT: // Float
            payload = PyFloat_FromDouble((double) bytes_to_long(tag, sizeof(float)));
            *moved += sizeof(float);
            break;

        case TAG_DOUBLE: // Double
            payload = PyFloat_FromDouble((double) bytes_to_long(tag, sizeof(double)));
            *moved += sizeof(double);
            break;

        case TAG_BYTE_ARRAY: // Byte array
            size = bytes_to_long(tag, sizeof(int));
            tag += sizeof(int);

            payload = PyByteArray_FromStringAndSize((char *) tag, size);
            *moved += sizeof(int) + size;
            break;

        case TAG_INT_ARRAY: // Int array
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

        case TAG_STRING: // String
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

        case TAG_LIST: // List
            list_id = tag[0];
            size = bytes_to_long(tag + 1, sizeof(int));
            tag += 1 + sizeof(int);

            payload = PyList_New(size);
            sub_moved = 0;
            for( i = 0; i < size; i++ )
            {
                PyObject * list_item;

                list_item = get_tag(tag, list_id, &sub_moved);
                PyList_SET_ITEM(payload, i, list_item);
                tag += sub_moved;
            }
            *moved += 5 + sub_moved;
            break;

        case TAG_COMPOUND: // Compound
            payload = PyDict_New();
            do // Repeatedly grab tags until an end tag is seen 
            {
                PyObject * sub_payload;
                unsigned char sub_id;
                char * sub_tag_name;
                int sub_tag_name_length;

                sub_id = *tag;
                if( sub_id == 0 )
                    break; // Found the end of our compound tag

                sub_tag_name_length = bytes_to_long(tag + 1, 2);

                // This should never happen, but the check doesn't hurt
                if( sub_tag_name_length == 0 )
                    return NULL; 

                sub_tag_name = calloc(sub_tag_name_length + 1, 1);
                strncpy(sub_tag_name, (char *) tag + 3, sub_tag_name_length);
                // printf("Tag ID: %d | Name: %.*s\n", sub_id, sub_tag_name_length, sub_tag_name);

                tag += 3 + sub_tag_name_length;
                sub_moved = 0;
                sub_payload = get_tag(tag, sub_id, &sub_moved);
                PyDict_SetItemString(payload, sub_tag_name, sub_payload);

                tag += sub_moved;
                *moved += 3 + sub_tag_name_length + sub_moved;

                free(sub_tag_name);
            }
            while( true );
            break;           

        default:
            printf("UNCAUGHT TAG ID: %d\n", id);
            return NULL;
    }

    Py_INCREF(payload);
    return payload;
}

// TODO: Going to ignore buffer size right now, but this isn't right
int write_tags( unsigned char * dst, PyObject * dict, int * moved )
{
    PyObject * keys;
    int i, size;

    keys = PyDict_Keys(dict);
    Py_INCREF(keys);

    size = PyDict_Size(dict);
    printf("size: %d\n", size);
    for( i = 0; i < size; i++ )
    {
        PyObject * key, * value;
        char * keystr;
        int j, len;
        struct TagType tag_info;

        key = PyList_GetItem(keys, i); // Known to be a PyString
        value = PyDict_GetItem(dict, key);
        keystr = PyString_AsString(key);

        // Match the tag name to a tag type
        for( j = 0;; j++ )
        {
            tag_info = leveldat_tags[j];
            if( tag_info.name == NULL)
            {
                PyErr_Format(PyExc_Exception, "\'%s\' is not a valid tag name", keystr);
                return -1;
            }

            if( strcmp(tag_info.name, keystr) == 0 )
                break;
        }

        printf("KEY: %s | NAME: %s | TAG_ID: %d\n", keystr, tag_info.name, tag_info.id);

        // Write the tag header
        len = strlen(tag_info.name);
        memcpy(dst, &tag_info.id, 1); // TODO: Might not be right, due to endianness
        memcpy(dst + 1, &len, 2);
        memcpy(dst + 3 , tag_info.name, len);

        dst += 3 + len;
        *moved += 3 + len;
        switch( tag_info.id )
        {
            long long_tmp;
            double double_tmp;
            char * byte_array_tmp;
            int k, size, sub_moved;

            case TAG_BYTE:
                long_tmp = PyInt_AsLong(value);
                memcpy(dst, &long_tmp, 1);
                dst += 1;
                *moved += 1;
                break;

            case TAG_SHORT:
                long_tmp = PyInt_AsLong(value);
                memcpy(dst, &long_tmp, sizeof(short));
                dst += sizeof(short);
                *moved += sizeof(short);
                break;

            case TAG_INT:
                long_tmp = PyInt_AsLong(value);
                memcpy(dst, &long_tmp, sizeof(int));
                dst += sizeof(int);
                *moved += sizeof(int);
                break;

            case TAG_LONG:
                long_tmp = PyInt_AsLong(value);
                memcpy(dst, &long_tmp, sizeof(long));
                dst += sizeof(long);
                *moved += sizeof(long);
                break;

            case TAG_FLOAT:
                double_tmp = PyFloat_AsDouble(value);
                memcpy(dst, &double_tmp, sizeof(float));
                dst += sizeof(float);
                *moved += sizeof(float);
                break;

            case TAG_DOUBLE:
                double_tmp = PyFloat_AsDouble(value);
                memcpy(dst, &double_tmp, sizeof(double));
                dst += sizeof(double);
                *moved += sizeof(double);
                break;

            case TAG_BYTE_ARRAY:
                size = PyByteArray_Size(value);
                memcpy(dst, &size, sizeof(int));

                byte_array_tmp = PyByteArray_AsString(value);
                memcpy(dst + sizeof(int), byte_array_tmp, size);

                dst += sizeof(int) + size;
                *moved += size;
                break;

            case TAG_INT_ARRAY:
                size = PyList_Size(value);
                memcpy(dst, &size, sizeof(int));
                dst += sizeof(int);

                for( k = 0; k < size; k++ )
                {
                    long_tmp = PyInt_AsLong(PyList_GetItem(value, k));
                    memcpy(dst, &long_tmp, sizeof(int));
                    dst += sizeof(int);
                }

                *moved += sizeof(int) * (size + 1);
                break;

            case TAG_STRING:
                // Special case, where a 'true' or 'false' has been changed
                // to a Python boolean object
                if( value == Py_True )
                    memcpy(dst, "true", 4);
                else if( value == Py_False )
                    memcpy(dst, "false", 5);
                else
                {
                    size = PyString_Size(value);
                    memcpy(dst, &size, sizeof(short));
                    dst += sizeof(short);

                    byte_array_tmp = PyString_AsString(value);
                    printf("STRING %p | %d\n", byte_array_tmp, size);
                    memcpy(dst, byte_array_tmp, 1);

                    dst += size;
                    *moved += sizeof(short) + size; 
                }
                break;

            case TAG_LIST:
                printf("TAG LIST, NOT IMPLEMENTED");
                break;

            case TAG_COMPOUND:
                sub_moved = 0;
                write_tags(dst, value, &sub_moved);
                memset(dst + sub_moved, 0, 1); // Write TAG_END

                dst += sub_moved;
                *moved += sub_moved + 1;
                break;

            default:
                PyErr_Format(PyExc_Exception, "\'%d\' is not a valid tag ID", tag_info.id);
                break;
        }
    }

    Py_DECREF(keys);
    return 0;
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

        inf(dst, src, size, 1);

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
