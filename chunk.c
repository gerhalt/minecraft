/*
chunk.c - Chunk definition

Provides functionality for Chunk objects
*/

#include <Python.h>
#include <structmember.h>
#include <stdbool.h>
#include "minecraft.h"

// Convert a chunk to python dictionary format
PyObject * chunk_to_dict( unsigned char * chunk_buffer )
{
    int moved;

    moved = 0;
    return get_tag(chunk_buffer, -1, &moved);
}

/*

Python object-related code

*/
void Chunk_dealloc( Chunk *self )
{
    // decrement references to PyObject *'s
    self->ob_type->tp_free((PyObject *) self);
}

static int Chunk_init( Chunk *self, PyObject *args, PyObject *kwds )
{
    if( !PyArg_ParseTuple(args, "ii", &self->chunk_x, &self->chunk_z) )
        return -1;

    return 0;
}

static PyMemberDef Chunk_members[] = {
    {"chunk_x", T_INT, offsetof(Chunk, chunk_x), 0, "Chunk X"},
    {"chunk_z", T_INT, offsetof(Chunk, chunk_z), 0, "Chunk Z"},
    {NULL}
};

static PyMethodDef Chunk_methods[] = {
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
