/*
generator.c

The initial version of the simplex-based generator.  Eventually this will
probably take the form of a full Python object, but for now I simply need
it to be usable from other modules.

Some code is derived from sources referenced on the wiki "Generator" write-up
*/

#include <Python.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <structmember.h>
#include "../minecraft.h"

#define PERMUTATIONS      256

float grad[12][3] = {
    {1.0, 1.0, 0.0}, {-1.0, 1.0, 0.0}, {1.0, -1.0, 0.0}, {-1.0, -1.0, 0.0},
    {1.0, 0.0, 1.0}, {-1.0, 0.0, 1.0}, {1.0, 0.0, -1.0}, {-1.0, 0.0, -1.0},
    {0.0, 1.0, 1.0}, {0.0, -1.0, 1.0}, {0.0, 1.0, -1.0}, {0.0, -1.0, -1.0}
};

typedef struct Generator {
    PyObject_HEAD
    int * perm;
    int seed;
} Generator;

// Helper function to calculate a dot product
float dot( float g[], float x, float y, float z )
{
    return g[0] * x + g[1] + y + g[2] * z;
}

float noise( Generator * g, int x, int y, int z )
{
    float n0, n1, n2, n3, F3, G3, s, t, x0, y0, z0, X0, Y0, Z0, x1, y1, z1, x2, y2, z2, x3, y3, z3, t0, t1, t2, t3;
    int i, j, k, i1, j1, k1, i2, j2, k2, ii, jj, kk, g0, g1, g2, g3;

    // Skew the input space to determine which simplex cell we're in
    F3 = 1.0 / 3.0;
    s = (x + y + z) * F3;
    i = x + s;
    j = y + s;
    k = z + s;

    // Unskew
    G3 = 1.0 / 6.0;
    t = (i + j + k) * G3;
    X0 = i - t;
    Y0 = j - t;
    Z0 = k - t;
    x0 = x - X0;
    y0 = y - Y0;
    z0 = z - Z0;

    // Simplexes are, in the 3D case, tetrahedrons, so we want to determine
    // which one we are in
    if(x0 >= y0)
    {
        if(y0 >= z0)      { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
        else if(x0 >= z0) { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 0; k2 = 1; }
        else              { i1 = 0; j1 = 0; k1 = 1; i2 = 1; j2 = 0; k2 = 1; }
    }
    else
    {
        if(y0 < z0)       { i1 = 0; j1 = 0; k1 = 1; i2 = 0; j2 = 1; k2 = 1; }
        else if(x0 < z0)  { i1 = 0; j1 = 1; k1 = 0; i2 = 0; j2 = 1; k2 = 1; }
        else              { i1 = 0; j1 = 1; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
    }

    // Calculate offsets for the three other corners of the simplex in XYZ
    x1 = x0 - i1 + G3;        // Second corner
    y1 = y0 - j1 + G3; 
    z1 = z0 - k1 + G3;
    x2 = x0 - i2 + 2.0 * G3;  // Third
    y2 = y0 - j2 + 2.0 * G3;
    z2 = z0 - k2 + 2.0 * G3;
    x3 = x0 - 1.0 + 3.0 * G3; // Fourth
    y3 = y0 - 1.0 + 3.0 * G3;
    z3 = z0 - 1.0 + 3.0 * G3;

    // Determine the corners' gradient indices
    ii = i & 0xFF;
    jj = j & 0xFF;
    kk = k & 0xFF;
    
    // Determine the corners' gradient values
    g0 = g->perm[ii + g->perm[jj + g->perm[kk]]] % 12;
    g1 = g->perm[ii + i1 + g->perm[jj + j1 + g->perm[kk + k1]]] % 12;
    g2 = g->perm[ii + i2 + g->perm[jj + j2 + g->perm[kk + k2]]] % 12;
    g3 = g->perm[ii + 1 + g->perm[jj + 1 + g->perm[kk + 1]]] % 12;

    // printf("Gradients: %d %d %d %d\n", g0, g1, g2, g3);

    // Calculate the contribution from each of the four corners
    t0 = 0.5 - x0 * x0 - y0 * y0 - z0 * z0;
    if(t0 < 0)
        n0 = 0.0;
    else
    {
        t0 *= t0;
        n0 = t0 * t0 * dot(grad[g0], x0, y0, z0);
    }

    t1 = 0.5 - x1 * x1 - y1 * y1 - z1 * z1;
    if(t1 < 0)
        n1 = 0.0;
    else
    {
        t1 *= t1;
        n1 = t1 * t1 * dot(grad[g1], x1, y1, z1);
    }

    t2 = 0.5 - x2 * x2 - y2 * y2 - z2 * z2;
    if(t2 < 0)
        n2 = 0.0;
    else
    {
        t2 *= t2;
        n2 = t2 * t2 * dot(grad[g2], x2, y2, z2);
    }

    t3 = 0.5 - x3 * x3 - y3 * y3 - z3 * z3;
    if(t3 < 0)
        n3 = 0.0;
    else
    {
        t3 *= t3;
        n3 = t3 * t3 * dot(grad[g3], x3, y3, z3);
    }

    // Return summed corner noise contributions
    // TODO: Unscaled for now
    return 16 * (n0 + n1 + n2 + n3);
}

/*
int main()
{
    Generator * g;
    float noise_value;
    int i, seed;

    seed = 123;
    g = setup(seed);
    
    for( i = 0; i < 100000; i++ )
    {
        noise_value = noise(g, (float) i, 0, 14);
//        printf("Noise value: %f\n", noise_value);
    }

    //Test permutation table generation
    
    for(int i = 0; i < PERMUTATIONS * 2; i++)
    {
        printf("%d, ", g->perm[i]);
    }
}
*/

/*

Python

*/
PyObject * Generator_noise( Generator *self, PyObject *args )
{
    PyObject *output;
    float x, y, z;

    if( !PyArg_ParseTuple(args, "fff", &x, &y, &z) )
    {
        PyErr_Format(PyExc_Exception, "Unable to parse arguments.");
        return NULL;
    }

    output = PyFloat_FromDouble((double) noise(self, x, y, z));
    Py_INCREF(output);
    return output;
}

void Generator_dealloc( Generator *self )
{
    free(self->perm);
    self->ob_type->tp_free((PyObject *) self);
}

int Generator_init( Generator *self, PyObject *args, PyObject *kwds )
{
    int i, seed;

    if( !PyArg_ParseTuple(args, "i", &seed) )
        return -1;

    self->seed = seed;

    // Generate the permutations table, doubled to avoid more work when indices wrap
    self->perm = calloc(sizeof(int), PERMUTATIONS * 2);
    for( i = PERMUTATIONS - 1; i >= 0; i-- )
    {
        int value, position;

        position = seed % PERMUTATIONS;
        value = self->perm[position];
        while(value > i) {
            position++;
            if(position > PERMUTATIONS - 1)
                position = 0;
            value = self->perm[position];
        }
        
        seed += i * 17;
        self->perm[position] = i;
        self->perm[position + PERMUTATIONS] = i;
    }

    return 0;
}

static PyMemberDef Generator_members[] = {
    {"seed", T_INT, offsetof(Generator, seed), 0, "Seed"},
    {NULL}
};

static PyMethodDef Generator_methods[] = {
    {"noise", (PyCFunction) Generator_noise, METH_VARARGS, "Get the noise strength at a particular point in three dimensional space."},
    {NULL}
};

PyTypeObject minecraft_GeneratorType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "minecraft.Generator",     /*tp_name*/
    sizeof(Generator),         /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)Generator_dealloc, /*tp_dealloc*/
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
    "Generator objects",       /* tp_doc */
    0,                     /* tp_traverse */
    0,                     /* tp_clear */
    0,                     /* tp_richcompare */
    0,                     /* tp_weaklistoffset */
    0,                     /* tp_iter */
    0,                     /* tp_iternext */
    Generator_methods,         /* tp_methods */
    Generator_members,         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Generator_init,  /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
