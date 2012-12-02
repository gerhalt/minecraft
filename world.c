/*
world.c

World object definition and functionality
*/

#include <Python.h>
#include <structmember.h>
#include <stdbool.h>
#include "minecraft.h"

#define MAX_REGIONS             8
#define NEW_REGION_BUFFER_SIZE  2000000
#define REGION_BUFFER_PADDING   10000

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
        dump_buffer(dst, 480);

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
    self->regions = NULL;

    return 0;
}

static PyObject * World_load_region( World *self, PyObject *args, PyObject *kwds )
{
    FILE * fp;
    Region * region;
    int region_x, region_z;
    char filename[1000]; // TODO: Dynamic

    if( !PyArg_ParseTuple(args, "ii", &region_x, &region_z) )
    {
        PyErr_Format(PyExc_Exception, "Cannot parse load_region parameters");

        Py_INCREF(Py_None);
        return Py_None;
    }

    sprintf(filename, "%s/r.%d.%d.mca", self->path, region_x, region_z);
    printf("Attempting to load %s\n", filename);

    region = malloc(sizeof(Region));

    fp = fopen(filename, "rb");
    if( fp == NULL )
    {
        printf("Cannot open region file, creating new buffer\n");
        // Create a new region buffer for the region
        region->buffer = calloc(NEW_REGION_BUFFER_SIZE, 1);
        region->buffer_size = NEW_REGION_BUFFER_SIZE;
        region->current_size = 0;
    }
    else
    {
        // Load the region into the buffer
        struct stat st;
        int size;

        stat(filename, &st);
        size = st.st_size + REGION_BUFFER_PADDING;
        region->buffer = calloc(size, 1);
        region->buffer_size = size;
        region->current_size = st.st_size;
    }
    region->x = region_x;
    region->z = region_z;
    region->next = self->regions;
    self->regions = region;

    print_region_info(region);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * World_load_chunk( World *self, PyObject *args, PyObject *kwds )
{
    // Make sure region file chunk would exist in exists
    return PyObject_CallObject((PyObject *) &minecraft_ChunkType, args); // Reference?
}

// Right now, just save out level.dat
static PyObject * World_save( World *self )
{
    FILE * fp;
    char filename[1000];
    unsigned char * compressed, * uncompressed;
    int size;

    uncompressed = calloc(10000, 1);
    compressed = calloc(5000, 1);
    size = write_tags(uncompressed, self->level);
    dump_buffer(uncompressed, 480);

    def(compressed, uncompressed, size, 1);

    sprintf(filename, "%s/level.dat", self->path);
    fp = fopen(filename, "wb");
    if( fp != NULL )
    {
        fwrite(compressed, size, 1, fp);

        fclose(fp);
    }

    free(compressed);
    free(uncompressed);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyMemberDef World_members[] = {
    {"path", T_STRING, offsetof(World, path), 0, "Path to the base minecraft world directory"},
    {"level", T_OBJECT, offsetof(World, level), 0, "Dictionary containing level.dat attributes"},
    {NULL}
};

static PyMethodDef World_methods[] = {
    {"save", (PyCFunction) World_save, METH_NOARGS, "Save the world! (out to file, anyway)"},
    {"load_chunk", (PyCFunction) World_load_chunk, METH_VARARGS, "Load a chunk."},
    {"load_region", (PyCFunction) World_load_region, METH_VARARGS, "Load a region."},
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
