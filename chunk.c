/*
chunk.c - Chunk definition

Provides functionality for Chunk objects
*/

#include <Python.h>
#include <structmember.h>
#include <stdbool.h>
#include "minecraft.h"

// Takes a region file stream and a chunk location and finds and decompresses
// the chunk to the passed buffer
int decompress_chunk( unsigned char * region, unsigned char * decompressed, int x, int z )
{
    unsigned int header_offset, chunk_offset, chunk_length, compression_type;
    unsigned char compressed_decompressed[1048576];

    printf("Finding chunk (%d, %d)\n", x, z);
    header_offset = 4 * ((x & 31) + (z & 31) * 32);

    chunk_offset = swap_endianness(region + header_offset, 3) * 4096;
    if ( chunk_offset == 0 )
        return 1;

    printf("Offset: %d | Length: %d\n", chunk_offset, *(region + header_offset + 3) * 4096);

    // Read the chunk length from the start of the chunk
    chunk_length = swap_endianness(region + chunk_offset, 4);
    compression_type = *(region + chunk_offset + 4);
    printf("True Length: %d | Compression: %d\n", chunk_length, compression_type);

    inf(decompressed, region + chunk_offset + 5, chunk_length - 1, 0);

    return 0;
}

/*

Python object-related code

*/
void Chunk_dealloc( Chunk *self )
{
    Py_XDECREF(self->world);
    Py_XDECREF(self->dict);
    self->ob_type->tp_free((PyObject *) self);
}

int Chunk_init( Chunk *self, PyObject *args, PyObject *kwds )
{
    Region * region;
    PyObject * old, * dict, * world;
    unsigned char buffer[100000]; // TODO: Dynamically allocate
    int moved, rc;

    if( !PyArg_ParseTuple(args, "Oii", &world, &self->x, &self->z) )
        return -1;

    region = load_region(world, self->x >> 5, self->z >> 5);
    rc = decompress_chunk(region->buffer, buffer, self->x, self->z);

    if( rc != 0 )
    {
        PyErr_Format(PyExc_Exception, "CHUNK EMPTY!");
        return -1;
    }

    // Read chunk to dictionary
    moved = 0;
    old = self->dict;
    dict = get_tag(buffer, -1, &moved);
    Py_INCREF(dict);
    self->dict = dict;
    Py_XDECREF(old);

    // Add reference to chunk's parent, the world
    old = self->world;
    Py_INCREF(world);
    self->world = world;
    Py_XDECREF(old);

    return 0;
}

static int Chunk_save( Chunk *self )
{
    /*
    1. Call load region
    2. Write chunk to temporary buffer
    3. Deflate chunk back to proper spot in region
    4. Done!
    */
    return 0;
}

static PyMemberDef Chunk_members[] = {
    {"world", T_OBJECT, offsetof(Chunk, world), 0, "World the chunk lives in"},
    {"dict", T_OBJECT, offsetof(Chunk, dict), 0, "Chunk attribute dictionary"},
    {"x", T_INT, offsetof(Chunk, x), 0, "Chunk X position"},
    {"z", T_INT, offsetof(Chunk, z), 0, "Chunk Z position"},
    {NULL}
};

static PyMethodDef Chunk_methods[] = {
    {"save", (PyCFunction) Chunk_save, METH_NOARGS, "Save the chunk to file"},
    {NULL}
};

PyTypeObject minecraft_ChunkType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "minecraft.Chunk",         /*tp_name*/
    sizeof(Chunk),             /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)Chunk_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Chunk objects",           /* tp_doc */
    0,                     /* tp_traverse */
    0,                     /* tp_clear */
    0,                     /* tp_richcompare */
    0,                     /* tp_weaklistoffset */
    0,                     /* tp_iter */
    0,                     /* tp_iternext */
    Chunk_methods,             /* tp_methods */
    Chunk_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Chunk_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
