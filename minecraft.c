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
