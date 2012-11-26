#include <Python.h>
#include <structmember.h>
#include <stdbool.h>
#include "minecraft.h"

// Given a pointer to a payload, return a PyObject representing that payload
// moved will be modified by the amount the tag pointer shifted
PyObject * get_tag( unsigned char * tag, char tag_id, int * moved )
{
    PyObject * payload;

    // The only time the tag_id should be -1 is if the root is passed in
    if( tag_id == -1 )
    {
        printf("ROOT TAG\n");
        tag_id = tag[0];
        tag += 3; // Skip the length of the same, since we know it to be 0
    }

    switch(tag_id)
    {
        long size;
        int i, sub_moved;
        unsigned char list_tag_id;

        case 1: // Byte
            payload = PyInt_FromLong(bytes_to_long(tag, 1));
            *moved += sizeof(char);
            break;

        case 2: // Short
            payload = PyInt_FromLong(bytes_to_long(tag, sizeof(short)));
            *moved += sizeof(short);
            break;

        case 3: // Int
            payload = PyInt_FromLong(bytes_to_long(tag, sizeof(int)));
            *moved += sizeof(int);
            break;

        case 4: // Long
            payload = PyInt_FromLong(bytes_to_long(tag, sizeof(long)));
            *moved += sizeof(long);
            break;

        case 5: // Float
            payload = PyFloat_FromDouble((double) bytes_to_long(tag, sizeof(float)));
            *moved += sizeof(float);
            break;

        case 6: // Double
            payload = PyFloat_FromDouble((double) bytes_to_long(tag, sizeof(double)));
            *moved += sizeof(double);
            break;

        case 7: // Byte array
            size = bytes_to_long(tag, sizeof(int));
            printf("Byte array size: %ld\n", size);
            payload = PyByteArray_FromStringAndSize((char *) tag + sizeof(int), size);
            *moved += sizeof(int) + size;
            break;

        case 11: // Int array
            size = bytes_to_long(tag, sizeof(int));
            tag += sizeof(int);

            payload = PyList_New(size);
            for( i = 0; i < size; i++ )
            {
                PyObject * integer;

                integer = PyInt_FromLong(bytes_to_long(tag, sizeof(int)));
                PyList_SET_ITEM(payload, i, integer);
                tag += sizeof(int);
            }
            *moved += sizeof(int) * (size + 1);
            break;

        case 8: // String
            size = bytes_to_long(tag, sizeof(short));
            payload = PyString_FromStringAndSize((char *) tag + sizeof(short), size);
            *moved += sizeof(short) + size;
            break;

        case 9: // List
            printf("LIST\n");
            list_tag_id = tag[0];
            size = bytes_to_long(tag + 1, sizeof(int));
            tag += 1 + sizeof(int);

            payload = PyList_New(size);
            sub_moved = 0;
            for( i = 0; i < size; i++ )
            {
                PyObject * list_item;

                list_item = get_tag(tag, list_tag_id, &sub_moved);
                PyList_SET_ITEM(payload, i, list_item);
                tag += sub_moved;
                printf("list item moved %d bytes", sub_moved);
            }
            *moved += 5 + sub_moved;
            break;

        case 10: // Compound
            payload = PyDict_New();
            printf("--> COMPOUND TAG, GOING DEEPER!\n");

            do // Repeatedly grab tags until an end tag is seen 
            {
                printf("GRABBING TAG\n");
                PyObject * sub_payload;
                unsigned char sub_tag_id;
                char sub_tag_name[1000]; // TODO: Non-dynamic length
                int sub_tag_name_length;

                sub_tag_id = *tag;
                if( sub_tag_id == 0 )
                    break; // Found the end of our compound tag

                sub_tag_name_length = bytes_to_long(tag + 1, 2);;

                unsigned char * name_buffer[sub_tag_name_length];
                strncpy(sub_tag_name, (char *) tag + 3, sub_tag_name_length);
                if( sub_tag_name_length == 0 )
                {
                    printf("Something is wrong, label missing");
                    return NULL;
                }

                printf("Tag ID: %d | Name: %.*s\n", sub_tag_id, sub_tag_name_length, sub_tag_name);

                tag += 3 + sub_tag_name_length;
                sub_moved = 0;
                sub_payload = get_tag(tag, sub_tag_id, &sub_moved);
                PyDict_SetItemString(payload, sub_tag_name, sub_payload);

                printf("%d bytes\n", sub_moved);
                tag += sub_moved;
                
                *moved += 3 + sub_tag_name_length + sub_moved;
            }
            while( true );

            printf("--> END OF COMPOUND TAG");
            break;           

        default:
            printf("UNCAUGHT TAG ID: %d\n", tag_id);
            payload = NULL;
            break;
    }
    return payload;
}

// Convert a chunk to python dictionary format
PyObject * chunk_to_dict( unsigned char * chunk_buffer )
{
    int moved;

    moved = 0;
    return get_tag(chunk_buffer, -1, &moved);
}

/*

Chunk definition

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
