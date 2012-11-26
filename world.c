/*
chunk.c - Chunk definition

Provides functionality for Chunk objects
*/

#include <Python.h>
#include <structmember.h>
#include <stdbool.h>
#include "minecraft.h"

/*

Python object-related code

*/
void World_dealloc( World *self )
{
    // decrement references to PyObject *'s
    self->ob_type->tp_free((PyObject *) self);
}

static int World_init( World *self, PyObject *args, PyObject *kwds )
{
    char * tmp;

    if( !PyArg_ParseTuple(args, "s", &tmp) )
       return -1;

    self->path = tmp;
}

static PyMemberDef World_members[] = {
    {"path", T_STRING, offsetof(World, path), 0, "Path to the base minecraft world directory"},
    {NULL}
};

static PyMethodDef World_methods[] = {
    {NULL}
};

PyTypeObject minecraft_WorldType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "minecraft.World",         /*tp_name*/
    sizeof(World),             /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)World_dealloc, /*tp_dealloc*/
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
    "World objects",           /* tp_doc */
    0,                     /* tp_traverse */
    0,                     /* tp_clear */
    0,                     /* tp_richcompare */
    0,                     /* tp_weaklistoffset */
    0,                     /* tp_iter */
    0,                     /* tp_iternext */
    World_methods,             /* tp_methods */
    World_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)World_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
