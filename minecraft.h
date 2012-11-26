/*
minecraft.h

Extension module header file
*/

typedef struct {
    PyObject_HEAD
    int chunk_x, chunk_z;
} Chunk;

typedef struct {
    PyObject_HEAD
    char *path; // path to the world
} World;

// chunk.c
PyTypeObject minecraft_ChunkType;

// minecraft.c
long bytes_to_long( unsigned char * buffer, int bytes );

// world.c
PyTypeObject minecraft_WorldType;
