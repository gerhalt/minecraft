/*

Header

*/

// chunk.c
typedef struct {
    PyObject_HEAD
    int chunk_x, chunk_z;
} Chunk;


PyTypeObject minecraft_ChunkType;

// minecraft.c
long bytes_to_long( unsigned char * buffer, int bytes );
