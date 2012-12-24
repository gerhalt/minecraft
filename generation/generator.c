/*
generator.c

The initial version of the simplex-based generator.  Eventually this will
probably take the form of a full Python object, but for now I simply need
it to be usable from other modules.

Some code is derived from sources referenced on the wiki "Generator" write-up
*/

#include <stdio.h>
#include <stdlib.h>

#define PERMUTATIONS      256

float grad[12][3] = {
    {1.0, 1.0, 0.0}, {-1.0, 1.0, 0.0}, {1.0, -1.0, 0.0}, {-1.0, -1.0, 0.0},
    {1.0, 0.0, 1.0}, {-1.0, 0.0, 1.0}, {1.0, 0.0, -1.0}, {-1.0, 0.0, -1.0},
    {0.0, 1.0, 1.0}, {0.0, -1.0, 1.0}, {0.0, 1.0, -1.0}, {0.0, -1.0, -1.0}
};

typedef struct Generator {
    int * perm;
} Generator;

// Helper function to calculate a dot product
float dot( float g[], float x, float y, float z )
{
    return g[0] * x + g[1] + y + g[2] * z;
}

Generator * setup( int seed )
{
    Generator * g;
    int i;

    g = malloc(sizeof(Generator));

    g->perm = calloc(sizeof(int), 256);
    for( i = 255; i >= 0; i-- )
    {
        int value, position;

        position = seed % 256;
        value = g->perm[position];
        while(value > i) {
            position++;
            if(position > 255)
                position = 0;
            value = g->perm[position];
        }
        
        seed += i * 17;
        g->perm[position] = i;
    }

    return g;
}

int noise( Generator * g, int x, int y, int z )
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
    ii = i & 255;
    jj = j & 255;
    kk = k & 255;
    
    // Determine the corners' gradient values
    g0 = g->perm[ii + g->perm[jj + g->perm[kk]]] % 12;
    g1 = g->perm[ii + i1 + g->perm[jj + j1 + g->perm[kk + k1]]] % 12;
    g2 = g->perm[ii + i2 + g->perm[jj + j2 + g->perm[kk + k2]]] % 12;
    g3 = g->perm[ii + 1 + g->perm[jj + 1 + g->perm[kk + 1]]] % 12;

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
    return n0 + n1 + n2 + n3;
}

int main()
{
    Generator * g;
    float noise_value;
    int i, seed;

    seed = 123;
    g = setup(seed);
    
    noise_value = noise(g, 10, 3901, 1239032);
    printf("Noise value: %f\n", noise_value);

    /* 
    Test permutation table generation
    
    for(int i = 0; i < 256; i++)
    {
        printf("%d, ", g->perm[i]);
    }
    */
}
