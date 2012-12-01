/*
chunk.c

Chunk object definition and functionality
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
    Py_XDECREF(self->level);
    self->ob_type->tp_free((PyObject *) self);
}

static int World_init( World *self, PyObject *args, PyObject *kwds )
{
    FILE * fp;
    char * tmp, filename[1000];
    unsigned char * src, * dst;

    if( !PyArg_ParseTuple(args, "s", &tmp) )
       return -1;

    sprintf(filename, "%s/level.dat", tmp);
    fp = fopen(filename, "rb");
    if( fp != NULL )
    {
        PyObject * level, * old_level;
        int size, moved, rc;
        struct stat stbuf;

        src = calloc(10000, 1);
        dst = calloc(10000, 1);

        rc = fstat(fileno(fp), &stbuf); 
        if( rc != 0 )
        {
            PyErr_Format(PyExc_Exception, "Unable to stat level.dat file");
            return -1;
        }

        fstat(fileno(fp), &stbuf);
        size = stbuf.st_size;

        fread(src, 1, size, fp);
        inf(dst, src, size, 1);

        printf("\n\nOriginal, unpacked data:\n");
        dump_buffer(dst, 200);

        moved = 0;
        level = get_tag(dst, -1, &moved);

        old_level = self->level;
        Py_INCREF(level);
        self->level = level;
        Py_XDECREF(old_level);

        free(src);
        free(dst);
        fclose(fp);
    }
    else
    {
        PyErr_Format(PyExc_Exception, "Unable to open level.dat file");
        return -1;
    }

    self->path = tmp;

    return 0;
}

// Right now, just save out level.dat
static PyObject * World_save( World *self )
{
    unsigned char * dst;
    int moved;

    dst = calloc(10000, 1);
    write_tags(dst, self->level);
    dump_buffer(dst, 200);

    free(dst);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyMemberDef World_members[] = {
    {"path", T_STRING, offsetof(World, path), 0, "Path to the base minecraft world directory"},
    {"level", T_OBJECT, offsetof(World, level), 0, "Dictionary containing level.dat attributes"},
    {NULL}
};

static PyMethodDef World_methods[] = {
    {"save", (PyCFunction) World_save, METH_NOARGS, "Save the world! (out to files, anyway)"},
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
