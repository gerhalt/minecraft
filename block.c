/*
block.c

Block object
*/

#include <Python.h>
#include <structmember.h>
#include <stdbool.h>
#include "minecraft.h"

/*

Python object-related code

*/
void Block_dealloc( Block *self )
{
    self->ob_type->tp_free((PyObject *) self);
}

int Block_init( Block *self, PyObject *args, PyObject *kwds )
{
    unsigned short id;
    unsigned char data, blocklight, skylight;

    if( !PyArg_ParseTuple(args, "HBBB", &id, &data, &blocklight, &skylight) )
        return -1;

    self->id = id; 
    self->data = data;
    self->blocklight = blocklight;
    self->skylight = skylight;

    return 0;
}

static PyMemberDef Block_members[] = {
    {"id", T_USHORT, offsetof(Block, id), 0, "Block ID, twelve bits"},
    {"data", T_UBYTE, offsetof(Block, data), 0, "Four bits of additional block data"},
    {"blocklight", T_UBYTE, offsetof(Block, blocklight), 0, "Four bits recording the amount of block-emitted light in each block"},
    {"skylight", T_UBYTE, offsetof(Block, skylight), 0, "Four bits recording the amount of sunlight or moonlight hitting each block"},
    {NULL}
};

static PyMethodDef Block_methods[] = {
    {NULL}
};

PyTypeObject minecraft_BlockType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "minecraft.Block",         /*tp_name*/
    sizeof(Block),             /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)Block_dealloc, /*tp_dealloc*/
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
    "Block objects",           /* tp_doc */
    0,                     /* tp_traverse */
    0,                     /* tp_clear */
    0,                     /* tp_richcompare */
    0,                     /* tp_weaklistoffset */
    0,                     /* tp_iter */
    0,                     /* tp_iternext */
    Block_methods,             /* tp_methods */
    Block_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Block_init,      /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
