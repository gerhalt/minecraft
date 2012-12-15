/*
chunk.c - Chunk definition

Provides functionality for Chunk objects
*/

#include <Python.h>
#include <structmember.h>
#include <stdbool.h>
#include "minecraft.h"
#include "tags.h"

// Takes a region file stream and a chunk location and finds and decompresses
// the chunk to the passed buffer
int decompress_chunk( unsigned char *region, unsigned char *decompressed, int x, int z )
{
    unsigned int header_offset, chunk_offset, chunk_length, compression_type;
    int rc;

    printf("Finding chunk (%d, %d)\n", x, z);
    header_offset = 4 *((x & 31) + (z & 31) *32);

    chunk_offset = swap_endianness(region + header_offset, 3) *4096;
    if ( chunk_offset == 0 )
    {
        printf("Chunk is empty, crap!\n");
        return 1;
    }

    printf("Offset: %d | Length: %d\n", chunk_offset, *(region + header_offset + 3) *4096);

    // Read the chunk length from the start of the chunk
    chunk_length = swap_endianness(region + chunk_offset, 4);
    compression_type = *(region + chunk_offset + 4);
    printf("True Length: %d | Compression: %d\n", chunk_length, compression_type);

    rc = inf(decompressed, region + chunk_offset + 5, chunk_length - 1, 0);

    if( rc < 0 )
    {
        PyErr_Format(PyExc_Exception, "Couldn't decompress!");
        return -1;
    }

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
    Region *region;
    PyObject *old, *dict, *world;
    unsigned char *buffer; // TODO: Dynamically allocate
    int moved, rc;

    if( !PyArg_ParseTuple(args, "Oii", &world, &self->x, &self->z) )
        return -1;

    buffer = calloc(1000000, 1);

    region = load_region((World *) world, self->x >> 5, self->z >> 5);
    rc = decompress_chunk(region->buffer, buffer, self->x, self->z);

    if( rc != 0 )
    {
        PyErr_Format(PyExc_Exception, "CHUNK EMPTY!");
        dump_buffer(buffer, 480);
        return -1;
    }

    // dump_buffer(buffer, 4800);

    // Read chunk to dictionary
    moved = 0;
    old = self->dict;
    dict = get_tag(buffer, -1, &moved);
    printf("Chunk moved: %d\n", moved);
    Py_INCREF(dict);
    self->dict = dict;
    Py_XDECREF(old);

    // Add reference to chunk's parent, the world
    old = self->world;
    Py_INCREF(world);
    self->world = world;
    Py_XDECREF(old);

    free(buffer);

    return 0;
}

static PyObject *Chunk_save( Chunk *self )
{
    Region *region;

    // Load the region, making sure we convert from chunk to region coordinates
    region = load_region((World *) self->world, self->x >> 5, self->z >> 5);
    update_region(region, self);

    Py_INCREF(Py_None);
    return Py_None;
}

char get_nibble( char *byte_array, int index )
{
    return index % 2 == 0 ? byte_array[index / 2] & 0x0F : byte_array[index / 2]>>4 & 0x0F;
}

void set_nibble( char *byte_array, int index, unsigned char value )
{
    char existing;

    value = value & 0x0F; // Ensure only the lower 4 bits are set
    value = index % 2 == 0 ? value : value << 4;
    existing = index % 2 == 0 ? byte_array[index / 2] & 0xF0 : byte_array[index / 2] & 0x0F;
    byte_array[index / 2] = existing | value;
}

// Currently, it is expected that passed arguments will be in coordinates
// relative to the chunk
PyObject *Chunk_get_block( Chunk *self, PyObject *args )
{
    PyObject *block_args, *block, *level, *sections, *section;
    int i, size, x, y, z;

    if( !PyArg_ParseTuple(args, "iii", &x, &y, &z) )
    {
        PyErr_Format(PyExc_Exception, "Unable to parse arguments.");

        Py_INCREF(Py_None);
        return Py_None;
    }

    level = PyDict_GetItemString(self->dict, "Level");
    sections = PyDict_GetItemString(level, "Sections");
    size = PyList_Size(sections);
    section = NULL;
    for( i = 0; i < size; i++ )
    {
        PyObject *current_section;
        int sub_y;

        current_section = PyList_GetItem(sections, i);
        sub_y = PyInt_AsLong(PyDict_GetItemString(current_section, "Y"));
        if( sub_y == y / 16 )
        {
            section = current_section;
            break;
        }
    }

    if( section == NULL )
        block_args = Py_BuildValue("HBBB", 0, 0, 0, 0);
    else
    {
        int section_y, position;
        unsigned short id;
        unsigned char data, blocklight, skylight;
        char *byte_array;

        data = blocklight = skylight = 0;
        section_y = y % 16;
        position = section_y *16 *16 + z *16 + x;

        // We know it will contain the "Blocks" byte array
        byte_array = (char *) PyDict_GetItemString(section, "Blocks");
        id = (short) byte_array[position];

        if( PyDict_Contains(section, PyString_FromString("Add")) )
        {
            byte_array = (char *) PyDict_GetItemString(section, "Add");
            id += get_nibble(byte_array, position) << 8;
        }

        if( PyDict_Contains(section, PyString_FromString("Data")) )
        {
            byte_array = (char *) PyDict_GetItemString(section, "Data");
            data = get_nibble(byte_array, position);
        }
        
        if( PyDict_Contains(section, PyString_FromString("BlockLight")) )
        {
            byte_array = (char *) PyDict_GetItemString(section, "BlockLight");
            blocklight = get_nibble(byte_array, position);
        }
        
        if( PyDict_Contains(section, PyString_FromString("SkyLight")) )
        {
            byte_array = (char *) PyDict_GetItemString(section, "SkyLight");
            skylight = get_nibble(byte_array, position);
        }

        block_args = Py_BuildValue("HBBB", id, data, blocklight, skylight);
    }

    // TODO: Populate with actual block values
    block_args = Py_BuildValue("HBBB", 0, 0, 0, 0);
    block = PyObject_CallObject((PyObject *) &minecraft_BlockType, block_args);
    Py_INCREF(block);
    return block;
}

// Currently, it is expected that passed arguments will be in coordinates
// relative to the chunk
PyObject *Chunk_put_block( Chunk *self, PyObject *args )
{
    PyObject *level, *sections, *section;
    Block *block;
    int i, size, position, x, y, z;
    char *byte_array;

    if( !PyArg_ParseTuple(args, "iiiO", &x, &y, &z, &block) )
    {
        PyErr_Format(PyExc_Exception, "Unable to parse arguments.");

        Py_INCREF(Py_None);
        return Py_None;
    }

    level = PyDict_GetItemString(self->dict, "Level");
    sections = PyDict_GetItemString(level, "Sections");
    size = PyList_Size(sections);
    section = NULL;
    for( i = 0; i < size; i++ )
    {
        PyObject *current_section;
        int sub_y;

        current_section = PyList_GetItem(sections, i);
        sub_y = PyInt_AsLong(PyDict_GetItemString(current_section, "Y"));
        if( sub_y == y / 16 )
        {
            section = current_section;
            break;
        }
    }

    // If a section doesn't exist where this block should go, create it
    if( section == NULL )
    {
        PyObject *new;

        new = PyInt_FromLong(y / 16); // This should end up not needing to be INCREF'd I think
        printf("Creating new section!\n");

        section = PyDict_New();
        Py_INCREF(section);
        
        Py_INCREF(new);
        PyDict_SetItemString(section, "Y", new);

        // Add doesn't necessarily exist, so we don't need to account for it

        // Data
        byte_array = calloc(4096, 1);
        new = PyByteArray_FromStringAndSize(byte_array, 4096);
        Py_INCREF(new);
        PyDict_SetItemString(section, "Blocks", new);
        free(byte_array);

        // Data, BlockLight, and SkyLight
        byte_array = calloc(2048, 1);
        new = PyByteArray_FromStringAndSize(byte_array, 2048);
        Py_INCREF(new);
        PyDict_SetItemString(section, "Data", new);
        new = PyByteArray_FromStringAndSize(byte_array, 2048);
        Py_INCREF(new);
        PyDict_SetItemString(section, "BlockLight", new);
        new = PyByteArray_FromStringAndSize(byte_array, 2048);
        Py_INCREF(new);
        PyDict_SetItemString(section, "SkyLight", new);
        free(byte_array);

        PyList_Append(sections, section);
    }

    position = (y % 16) *16 *16 + z *16 + x;
    byte_array = PyByteArray_AsString(PyDict_GetItemString(section, "Blocks"));
    byte_array[position] = block->id;

    // set "Add", only if the block ID is greater than 0x0F
    if( block->id >> 8 != 0 )
    {
        if( PyDict_Contains(section, PyString_FromString("Add")) == 0 )
        {
            PyObject *new;

            byte_array = calloc(2048, 1);
            new = PyByteArray_FromStringAndSize(byte_array, 2048);
            Py_INCREF(new);

            PyDict_SetItemString(section, "Add", new);
            free(byte_array);
        }

        byte_array = PyByteArray_AsString(PyDict_GetItemString(section, "Add"));
        set_nibble(byte_array, position, block->data >> 8);
    }

    byte_array = PyByteArray_AsString(PyDict_GetItemString(section, "Data"));
    set_nibble(byte_array, position, block->data);
    byte_array = PyByteArray_AsString(PyDict_GetItemString(section, "BlockLight"));
    set_nibble(byte_array, position, block->blocklight);
    byte_array = PyByteArray_AsString(PyDict_GetItemString(section, "SkyLight"));
    set_nibble(byte_array, position, block->skylight);

    Py_INCREF(Py_None);
    return Py_None;
}

// TODO: Placeholder
// Recalculate lighting and anything else that requires calculation
static PyObject *Chunk_calculate( Chunk *self )
{
    Py_INCREF(Py_None);
    return Py_None;
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
    {"get_block", (PyCFunction) Chunk_get_block, METH_VARARGS, "Get a block from within the chunk"},
    {"put_block", (PyCFunction) Chunk_put_block, METH_VARARGS, "Put a block into the chunk, at the given location"},
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
