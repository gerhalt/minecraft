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
#include "tags.h"
#include "zlib.h"

TagType leveldat_tags[] = {
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

TagType chunk_tags[] = {
    // Basic chunk tags
    {"Level", TAG_COMPOUND},
    {"xPos", TAG_INT},
    {"zPos", TAG_INT},
    {"LastUpdate", TAG_LONG},
    {"TerrainPopulated", TAG_BYTE},
    {"Biomes", TAG_BYTE_ARRAY},
    {"HeightMap", TAG_INT_ARRAY},
    {"Sections", TAG_LIST, false, TAG_COMPOUND},
    {"Y", TAG_BYTE},
    {"Blocks", TAG_BYTE_ARRAY},
    {"Add", TAG_BYTE_ARRAY},
    {"Data", TAG_BYTE_ARRAY},
    {"BlockLight", TAG_BYTE_ARRAY},
    {"SkyLight", TAG_BYTE_ARRAY},
    {"Entities", TAG_LIST, true, TAG_BYTE_ARRAY},
    {"TileEntities", TAG_LIST, true, TAG_BYTE_ARRAY},
    {"TileTicks", TAG_LIST, false, TAG_COMPOUND},
    // TODO: Fill in other tags
    {NULL, NULL}
};

int write_tags_header( unsigned char * dst, PyObject * dict, TagType tags[], int * moved );

// Takes a number of bytes (in big-endian order), starting at a given location,
// and returns the integer they represent
long swap_endianness( unsigned char * buffer, int bytes )
{
    long transform;
    int i;

    transform = 0;
    for( i = 0; i < bytes; i++ )
    {
        transform = buffer[i]<<((bytes - i - 1) * 8) | transform;
    }
    return transform;
}

void swap_endianness_in_memory( unsigned char * buffer, int bytes )
{
    unsigned char * end;

    end = buffer + bytes - 1;
    while( buffer < end )
    {
        unsigned char tmp;
 
//        printf("Swapping %x and %x\n", *buffer, *end);
        tmp = *buffer;
        *buffer = *end;
        *end = tmp;
//        printf("Now %x and %x\n", *buffer, *end);

        buffer++;
        end--;
    }
}

// Takes a buffer and prints it prettily
void dump_buffer( unsigned char * buffer, int count )
{
    char string[16];
    int i, j;

    for( i = 0; i < count; i++)
    {
        if ( i % 16 == 0 )
            printf(" %4i ", i); 

        printf("%02x", buffer[i]);
        if( buffer[i] >= 0x20 && buffer[i] <= 0x7E )
            string[i % 16] = buffer[i];
        else
            string[i % 16] = 0x2E;

        if ( i % 4 == 3 )
            printf(" ");
        if ( i % 16 == 15 )
            printf(" |%.*s|\n", 16, string);
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
int def( unsigned char * dst, unsigned char * src, int bytes, int mode, int * size )
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

    *size = strm.total_out;
    return ret;
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
            payload = PyInt_FromLong(swap_endianness(tag, 1));
            *moved += sizeof(char);
            break;

        case TAG_SHORT: // Short
            payload = PyInt_FromLong(swap_endianness(tag, sizeof(short)));
            *moved += sizeof(short);
            break;

        case TAG_INT: // Int
            payload = PyInt_FromLong(swap_endianness(tag, sizeof(int)));
            *moved += sizeof(int);
            break;

        case TAG_LONG: // Long
            payload = PyInt_FromLong(swap_endianness(tag, sizeof(long)));
            *moved += sizeof(long);
            break;

        case TAG_FLOAT: // Float
            payload = PyFloat_FromDouble((double) swap_endianness(tag, sizeof(float)));
            *moved += sizeof(float);
            break;

        case TAG_DOUBLE: // Double
            payload = PyFloat_FromDouble((double) swap_endianness(tag, sizeof(double)));
            *moved += sizeof(double);
            break;

        case TAG_BYTE_ARRAY: // Byte array
            size = swap_endianness(tag, sizeof(int));
            tag += sizeof(int);

            payload = PyByteArray_FromStringAndSize((char *) tag, size);
            *moved += sizeof(int) + size;
            break;

        case TAG_INT_ARRAY: // Int array
            size = swap_endianness(tag, sizeof(int));
            tag += sizeof(int);

            payload = PyList_New(size);
            for( i = 0; i < size; i++ )
            {
                PyObject * integer;

                integer = PyInt_FromLong(swap_endianness(tag, sizeof(int)));
                PyList_SET_ITEM(payload, i, integer);
                tag += sizeof(int);
            }
            *moved += sizeof(int) * (size + 1);
            break;

        case TAG_STRING: // String
            size = swap_endianness(tag, sizeof(short));
            tag += sizeof(short);

            payload = PyString_FromStringAndSize((char *) tag, size);

            // Take care of the special case where a boolean value is 
            // represented as a string, and convert to a Python boolean object
            if( memcmp(PyString_AsString(payload), "true", size) )
                payload = Py_True;
            else if( memcmp(PyString_AsString(payload), "false", size)  )
                payload = Py_False;

            *moved += sizeof(short) + size;
            break;

        case TAG_LIST: // List
            list_id = tag[0];
            size = swap_endianness(tag + 1, sizeof(int));
            tag += 1 + sizeof(int);
            *moved += 1 + sizeof(int);

            payload = PyList_New(size);
            for( i = 0; i < size; i++ )
            {
                PyObject * list_item;

                sub_moved = 0;
                list_item = get_tag(tag, list_id, &sub_moved);
                PyList_SET_ITEM(payload, i, list_item);
                tag += sub_moved;
                *moved += sub_moved;
            }
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
                {
                    *moved += 1;
                    break; // Found the end of our compound tag
                }

                sub_tag_name_length = swap_endianness(tag + 1, 2);

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

/*
write_tags
  * dst - destination buffer
  * dict - Python dictionary to convert
returns
  total size of written tags, including root tag
*/
int write_tags( unsigned char * dst, PyObject * dict, TagType tags[] )
{
    int moved;

    // Write the root compound tag
    memset(dst, TAG_COMPOUND, 1);
    memset(dst + 1, 0, sizeof(short));
    dst += 1 + sizeof(short);

    moved = 0;
    write_tags_header(dst, dict, tags, &moved);
    dst += moved;

    // Write the end tag
    memset(dst, 0, 1);

    printf("MOVED: %d\n", moved + 4);
    return moved + 4; // Size of subtags + 4 bytes for root tag
}


int write_tags_payload( unsigned char * dst, TagType tag_info, PyObject * payload, TagType tags[], int * moved )
{
    // printf("Name: %s | ID: %d\n", tag_info.name, tag_info.id);
    switch(tag_info.id)
    {
        PyObject * list_item_tmp;
        long long_tmp;
        double double_tmp;
        char * byte_array_tmp;
        int i, size, sub_moved;

        case TAG_BYTE:
            long_tmp = PyInt_AsLong(payload);
            memcpy(dst, &long_tmp, 1);
            *moved += 1;
            break;

        case TAG_SHORT:
            long_tmp = PyInt_AsLong(payload);
            memcpy(dst, &long_tmp, sizeof(short));
            swap_endianness_in_memory(dst, 2);
            *moved += sizeof(short);
            break;

        case TAG_INT:
            long_tmp = PyInt_AsLong(payload);
            memcpy(dst, &long_tmp, sizeof(int));
            swap_endianness_in_memory(dst, 4);
            *moved += sizeof(int);
            break;

        case TAG_LONG:
            long_tmp = PyInt_AsLong(payload);
            memcpy(dst, &long_tmp, sizeof(long));
            swap_endianness_in_memory(dst, 8);
            *moved += sizeof(long);
            break;

        case TAG_FLOAT:
            double_tmp = PyFloat_AsDouble(payload);
            memcpy(dst, &double_tmp, sizeof(float));
            swap_endianness_in_memory(dst, 4);
            *moved += sizeof(float);
            break;

        case TAG_DOUBLE:
            double_tmp = PyFloat_AsDouble(payload);
            memcpy(dst, &double_tmp, sizeof(double));
            swap_endianness_in_memory(dst, 8);
            *moved += sizeof(double);
            break;

        case TAG_BYTE_ARRAY:
            size = PyByteArray_Size(payload);
            memcpy(dst, &size, sizeof(int));
            swap_endianness_in_memory(dst, 4);

            byte_array_tmp = PyByteArray_AsString(payload);
            memcpy(dst + sizeof(int), byte_array_tmp, size);

            *moved += size;
            break;

        case TAG_INT_ARRAY:
            size = PyList_Size(payload);
            memcpy(dst, &size, sizeof(int));
            swap_endianness_in_memory(dst, 4);
            dst += sizeof(int);

            for( i = 0; i < size; i++ )
            {
                long_tmp = PyInt_AsLong(PyList_GetItem(payload, i));
                memcpy(dst, &long_tmp, sizeof(int));
                swap_endianness_in_memory(dst, 4);
                dst += sizeof(int);
            }

            *moved += sizeof(int) * (size + 1);
            break;

        case TAG_STRING:
            // Special case, where a 'true' or 'false' has been changed
            // to a Python boolean object
            if( payload == Py_True )
            {
                size = 4;
                byte_array_tmp = "true";
            }
            else if( payload == Py_False )
            {
                size = 5;
                byte_array_tmp = "false";
            }
            else
            {
                size = PyString_Size(payload);
                byte_array_tmp = PyString_AsString(payload);
            }

            memcpy(dst, &size, sizeof(short));
            swap_endianness_in_memory(dst, 2);
            memcpy(dst + sizeof(short), byte_array_tmp, size);

            *moved += sizeof(short) + size; 
            break;

        case TAG_LIST:
            // If we hit this, the extra fields for the tag_info should be set
            size = PyList_Size(payload);

            // Special case: if the list tag is empty, some list tags will 
            // still be written to the file as a byte array
            if( size == 0 && tag_info.empty_byte_list )
            {
                *dst = TAG_BYTE_ARRAY; 
                *moved += 5; // 1 + 4 for the empty byte array
                break;
            }

            memcpy(dst + 1, &size, 4);
            swap_endianness_in_memory(dst + 1, 4);
            dst += 5;
            *moved += 5;

            for( i = 0; i < size; i++ )
            {
                TagType sub_tag_info;

                sub_moved = 0;
                sub_tag_info.name = ""; // Sub tag name doesn't matter
                sub_tag_info.id = tag_info.sub_tag_id;
                list_item_tmp = PyList_GetItem(payload, i);
                write_tags_payload(dst, sub_tag_info, list_item_tmp, tags, &sub_moved);

                dst += sub_moved;
                *moved += sub_moved;
            }
            break;

        case TAG_COMPOUND:
            sub_moved = 0;
            write_tags_header(dst, payload, tags, &sub_moved);
            memset(dst + sub_moved, 0, 1); // Write TAG_END
            *moved += sub_moved + 1;
            break;

        default:
            PyErr_Format(PyExc_Exception, "\'%d\' is not a valid tag ID", tag_info.id);
            break;
    }
    return 0;
}

// TODO: Buffer size might be an issue
int write_tags_header( unsigned char * dst, PyObject * dict, TagType tags[], int * moved )
{
    PyObject * keys;
    int i, size;

    keys = PyDict_Keys(dict);
    Py_INCREF(keys);

    size = PyDict_Size(dict);
    for( i = 0; i < size; i++ )
    {
        PyObject * key, * value;
        char * keystr;
        int j, len, sub_moved;
        struct TagType tag_info;

        key = PyList_GetItem(keys, i); // Known to be a PyString
        value = PyDict_GetItem(dict, key);
        keystr = PyString_AsString(key);

        // Match the tag name to a tag type
        for( j = 0;; j++ )
        {
            tag_info = tags[j];
            if( tag_info.name == NULL)
            {
                PyErr_Format(PyExc_Exception, "\'%s\' is not a valid tag name", keystr);
                return -1;
            }

            if( strcmp(tag_info.name, keystr) == 0 )
                break;
        }

        // printf("KEY: %s | NAME: %s | TAG_ID: %d\n", keystr, tag_info.name, tag_info.id);

        // Write the tag header
        len = strlen(tag_info.name);
        memcpy(dst, &tag_info.id, 1);
        memcpy(dst + 1, &len, 2); 
        swap_endianness_in_memory(dst + 1, 2);
        memcpy(dst + 3, tag_info.name, len);

        // Write the tag
        sub_moved = 0;
        write_tags_payload(dst + 3 + len, tag_info, value, tags, &sub_moved);

        dst += 3 + len + sub_moved;
        *moved += 3 + len + sub_moved;
    }
    Py_DECREF(keys);
    return 0;
}
