/*
minecraft.c

Core module definition plus functionality that doesn't really fit anywhere else
*/

#include <Python.h>
#include <structmember.h>
#include <regex.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "minecraft.h"

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
            tag += sizeof(int);

            printf("Byte array size: %ld\n", size);
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
                char * sub_tag_name;
                int sub_tag_name_length;

                sub_tag_id = *tag;
                if( sub_tag_id == 0 )
                    break; // Found the end of our compound tag

                sub_tag_name_length = bytes_to_long(tag + 1, 2);;

                if( sub_tag_name_length == 0 )
                {
                    printf("Something is wrong, label missing");
                    return NULL;
                }

                sub_tag_name = calloc(sub_tag_name_length + 1, 1);
                strncpy(sub_tag_name, (char *) tag + 3, sub_tag_name_length);
                printf("Tag ID: %d | Name: %.*s\n", sub_tag_id, sub_tag_name_length, sub_tag_name);

                tag += 3 + sub_tag_name_length;
                sub_moved = 0;
                sub_payload = get_tag(tag, sub_tag_id, &sub_moved);
                PyDict_SetItemString(payload, sub_tag_name, sub_payload);

                printf("%d bytes\n", sub_moved);
                tag += sub_moved;
                
                *moved += 3 + sub_tag_name_length + sub_moved;

                free(sub_tag_name);
            }
            while( true );

            printf("--> END OF COMPOUND TAG");
            break;           

        default:
            printf("UNCAUGHT TAG ID: %d\n", tag_id);
            return NULL;
    }

    Py_INCREF(payload);
    return payload;
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

/*
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

    if ( (fp = fopen(argv[1], "r")) )
    {
        decompress_chunk(fp, chunk, 0, 5);
//        chunk_to_dict(chunk);
    }

    fclose(fp);

    return 0;
}
*/
