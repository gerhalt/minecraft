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
int decompress_chunk( FILE * region_file, unsigned char * chunk_buffer, int chunkX, int chunkZ )
{
    unsigned int offset, chunk_length, compression_type;
    unsigned char small_buffer[5], compressed_chunk_buffer[1048576];

    printf("Finding chunk (%d, %d)\n", chunkX, chunkZ);
    fseek(region_file, 4 * ((chunkX & 31) + (chunkZ & 31) * 32), SEEK_SET);
    fread(small_buffer, 4, 1, region_file);

    offset = swap_endianness(small_buffer, 3) * 4096;
    if ( offset == 0 )
        return 1;

    printf("Offset: %d | Length: %d\n", offset, small_buffer[3] * 4096);

    // Seek to start of chunk, copy it into buffer
    fseek(region_file, offset, SEEK_SET);
    memset(small_buffer, 0, 5);
    fread(small_buffer, 5, 1, region_file);
    chunk_length = swap_endianness(small_buffer, 4);
    compression_type = swap_endianness(small_buffer + 4, 1);
    printf("Actual Length: %d\n", chunk_length);
    printf("Compression scheme: %d\n", compression_type);

    fread(compressed_chunk_buffer, chunk_length - 1, 1, region_file);
    inf(chunk_buffer, compressed_chunk_buffer, chunk_length - 1, 0);

    return 0;
}

/*

Python object-related code

*/
void Chunk_dealloc( Chunk *self )
{
    Py_DECREF(self->dict);
    self->ob_type->tp_free((PyObject *) self);
}

/*
This is probably an inefficient first try.  Basically the plan is:
1. Check if the region file exists
2. If it doesn't, create an empty chunk (should still be able to save out)
3. If it does, we can load it as normal
*/
static int Chunk_init( Chunk *self, PyObject *args, PyObject *kwds )
{
    FILE * fp;
    PyObject *dict, * old_dict;
    unsigned char filename[1000], buffer[100000]; // TODO: Temporary
    int moved, region_x, region_z;

    if( !PyArg_ParseTuple(args, "ii", &self->x, &self->z) )
        return -1;

    region_x = self->x >> 5;
    region_z = self->z >> 5;
    sprintf(filename, "/home/josh/minecraft_new/world/region/r.%d.%d.mca", region_x, region_z);
    printf("Going to attempt to open %s", filename);
    
    fp = fopen(filename, "rb");
    if( fp != NULL )
    {
        decompress_chunk(fp, buffer, self->x, self->z);
        dump_buffer(buffer, 480);
    }
    // else region file doesn't exist, which is ok
    
    moved = 0;
    old_dict = self->dict;
    dict = get_tag(buffer, -1, &moved);
    Py_INCREF(dict);
    self->dict = dict;
    Py_XDECREF(old_dict);

    return 0;
}

static int Chunk_save( Chunk *self )
{

}

static PyMemberDef Chunk_members[] = {
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
