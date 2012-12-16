/*
world.c

World object definition and functionality
*/

#include <Python.h>
#include <structmember.h>
#include <stdbool.h>
#include "minecraft.h"
#include "tags.h"

/*
Ensures that a region is in memory, or copies the file into memory if it isn't.
If the region doesn't exist in file form, simply creates a new region in
memory, which can then be saved out.

Currently wrapped by World_load_region(...), though this function may end up
being deprecated.
*/
Region *load_region( World *self, int x, int z )
{
    FILE *fp;
    Region *region;
    char filename[1000]; // TODO: Dynamic
    int count;

    // Check to make sure the region isn't already in memory, and count we'll
    // we're at it (for use below)
    region = self->regions;
    count = 0;
    while( region != NULL )
    {
        if( region->x == x && region->z == z )
        {
            printf("Region already in memory!\n");
            return region;
        }
        region = region->next;
        count++;
    }

    // Ensure we aren't up against the maximum number of regions in memory - if
    // we are, we'll boot the least-recently used one
    if( count >= MAX_REGIONS )
    {
        printf("Hit max number of regions in memory, discarding last\n");
        region = self->regions;
        count = 0;
        do {
            region = region->next;
            count++;
        } while( count < MAX_REGIONS );
        // Chop off the end
        unload_region(region->next, self->path);
        region->next = NULL;
    }

    sprintf(filename, "%s/region/r.%d.%d.mca", self->path, x, z);
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

        fread(region->buffer, 1, st.st_size, fp);

        fclose(fp);
    }
    region->x = x;
    region->z = z;
    region->next = self->regions;
    self->regions = region;

    print_region_info(region);

    return region;
}

/*
Helper functionality which looks for a chunk in the simple hash table a World
contains.  If the location that chunk would be at is empty, or occupied by a 
different Chunk with the same signature, the new chunk is pulled in (and the 
old chunk is saved, if needed).
*/
PyObject * get_chunk( World *world, int x, int z )
{
    PyObject *chunk;
    int hash;

    hash = (x << 16 + z) % MAX_CHUNKS;
    printf("Chunk hash: %d\n", hash);

    chunk = world->chunks[hash];
    if( chunk == NULL || (((Chunk *) chunk)->x != x || ((Chunk *) chunk)->z != z) )
    {
        PyObject *chunk_args;

        printf("Chunk not found, or wrong chunk, loading\n");

        Py_XDECREF(chunk);
        chunk_args = Py_BuildValue("Oii", (PyObject *) world, x, z);
        chunk = PyObject_CallObject((PyObject *) &minecraft_ChunkType, chunk_args);

        Py_INCREF(chunk); // Table entry reference
        world->chunks[hash] = chunk;
    }

    Py_INCREF(chunk); 
    return chunk;
}

/*

Python object-related code

*/
void World_dealloc( World *self )
{
    int i;

    for( i = 0; i < MAX_CHUNKS; i++ )
        Py_XDECREF(self->chunks[i]);
    free(self->chunks);

    Py_XDECREF(self->level);
    self->ob_type->tp_free((PyObject *) self);
}

static int World_init( World *self, PyObject *args, PyObject *kwds )
{
    FILE *fp;
    char *tmp, filename[1000];
    unsigned char *src, *dst;

    if( !PyArg_ParseTuple(args, "s", &tmp) )
       return -1;

    sprintf(filename, "%s/level.dat", tmp);
    fp = fopen(filename, "rb");
    if( fp != NULL )
    {
        PyObject *level, *old_level;
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

    // Set up table to store chunks that are in memory
    self->chunks = calloc(sizeof(PyObject *), MAX_CHUNKS);

    return 0;
}

/*
Get a block in the world
*/
static PyObject *World_get_block( World *self, PyObject *args, PyObject *kwds )
{
    PyObject *chunk, *block, *block_args;
    int i, x, y, z;

    if( !PyArg_ParseTuple(args, "iii", &x, &y, &z) )
    {
        PyErr_Format(PyExc_Exception, "Cannot parse parameters");

        Py_INCREF(Py_None);
        return Py_None;
    }

    y = y % 256;

    chunk = get_chunk(self, x >> 4, z >> 4);

    block_args = Py_BuildValue("iii", x % 16, y, z % 16); 
    block = Chunk_get_block((Chunk *) chunk, block_args);
    Py_INCREF(block);
    return block;
}

PyObject *World_put_block( World *self, PyObject *args )
{
    PyObject *chunk, *block, *block_args;
    int x, y, z;

    if( !PyArg_ParseTuple(args, "iiiO", &x, &y, &z, &block) )
    {
        PyErr_Format(PyExc_Exception, "Cannot parse parameters");

        Py_INCREF(Py_None);
        return Py_None;
    }

    y = y % 256;

    chunk = get_chunk(self, x >> 4, z >> 4);

    block_args = Py_BuildValue("iiiO", x % 16, y, z % 16, (PyObject *) block); 
    Chunk_put_block((Chunk *) chunk, block_args);

    Py_INCREF(Py_None);
    return Py_None;
}

// TODO: Re-evalute, moving this to a wrapper
static PyObject *World_load_region( World *self, PyObject *args, PyObject *kwds )
{
    int region_x, region_z;

   if( !PyArg_ParseTuple(args, "ii", &region_x, &region_z) )
    {
        PyErr_Format(PyExc_Exception, "Cannot parse load_region parameters");

        Py_INCREF(Py_None);
        return Py_None;
    }

    load_region(self, region_x, region_z);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *World_save_region( World *self, PyObject *args, PyObject *kwds )
{
    Region *region;
    int region_x, region_z;

    if( !PyArg_ParseTuple(args, "ii", &region_x, &region_z) )
    {
        PyErr_Format(PyExc_Exception, "Cannot parse load_region parameters");

        Py_INCREF(Py_None);
        return Py_None;
    }

    region = self->regions;
    while( region != NULL )
    {
        if( region->x == region_x && region->z == region_z )
            break;
        region = region->next;
    }

    if( region == NULL)
        printf("Region (%d, %d) not loaded!\n", region_x, region_z);
    else
    {
        int i;

        // Save any chunks in memory
        for( i = 0; i < MAX_CHUNKS; i++ )
        {
            Chunk *chunk;
            chunk = (Chunk *) self->chunks[i];
            if( chunk == NULL )
                continue;
            else if( chunk->x >> 5 == region_x && chunk->z >> 5 == region_z )
            {
                printf("Chunk %d,%d saved during region save\n", chunk->x, chunk->z);
                update_region(region, chunk);
            }
        }

        save_region(region, self->path); 
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *World_load_chunk( World *self, PyObject *args )
{
    PyObject *chunk, *chunk_args;
    int x, z;

    if( !PyArg_ParseTuple(args, "ii", &x, &z) )
    {
        PyErr_Format(PyExc_Exception, "Cannot parse parameters");

        Py_INCREF(Py_None);
        return Py_None;
    }

    chunk_args = Py_BuildValue("Oii", (PyObject *) self, x, z);
    chunk = PyObject_CallObject((PyObject *) &minecraft_ChunkType, chunk_args);
    Py_DECREF(chunk_args);

    return chunk;
}

// Right now, just save out level.dat
static PyObject *World_save( World *self )
{
    FILE *fp;
    char filename[1000];
    unsigned char *compressed, *uncompressed;
    int size, deflated_size;

    uncompressed = calloc(10000, 1);
    compressed = calloc(5000, 1);
    size = write_tags(uncompressed, self->level, leveldat_tags);
    dump_buffer(uncompressed, 480);

    deflated_size = 0;
    def(compressed, uncompressed, size, 1, &deflated_size);

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
    {"get_block", (PyCFunction) World_get_block, METH_VARARGS, "Get the block at a given location."},
    {"put_block", (PyCFunction) World_put_block, METH_VARARGS, "Put a block at a given spot."},
    {"load_region", (PyCFunction) World_load_region, METH_VARARGS, "Load a region."},
    {"save_region", (PyCFunction) World_save_region, METH_VARARGS, "Save a region, assuming it has been modified and is in memory"},
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
