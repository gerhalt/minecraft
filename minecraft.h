/*
minecraft.h

Extension module header file
*/

static int SMALL_DEFLATE_MAX = 10000;
static int SMALL_INFLATE_MAX = 20000;
static int CHUNK_DEFLATE_MAX = 65536;
static int CHUNK_INFLATE_MAX = 131072;

// structs
typedef struct {
    PyObject_HEAD
    int chunk_x, chunk_z;
} Chunk;

typedef struct {
    PyObject_HEAD
    PyObject * level; // level.dat dictionay
    char * path;      // path to the world
} World;

// chunk.c
PyTypeObject minecraft_ChunkType;

// minecraft.c
long bytes_to_long( unsigned char * buffer, int bytes );

// nbt.c
int inf( unsigned char * dst, unsigned char * src, int bytes, int mode );
int def( unsigned char * dst, unsigned char * src, int bytes, int mode );
PyObject * get_tag( unsigned char * tag, char tag_id, int * moved );
int write_tags( unsigned char * dst, PyObject * dict, int * moved ); 

// world.c
PyTypeObject minecraft_WorldType;
