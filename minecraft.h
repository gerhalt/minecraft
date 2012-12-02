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
    PyObject * dict;
    int x, z;
} Chunk;

typedef struct {
    unsigned char * buffer;
    int x, z, buffer_size, current_size;
    struct Region * next; // Meant to function as a linked list, as part of the World
} Region;

typedef struct {
    PyObject_HEAD
    PyObject * level; // level.dat dictionary
    char * path;      // path to the world
    Region * regions;
} World;

// chunk.c
PyTypeObject minecraft_ChunkType;

// nbt.c
long swap_endianness( unsigned char * buffer, int bytes );
void swap_endianness_in_memory( unsigned char * buffer, int bytes );
void dump_buffer( unsigned char * buffer, int count );
int inf( unsigned char * dst, unsigned char * src, int bytes, int mode );
int def( unsigned char * dst, unsigned char * src, int bytes, int mode );
PyObject * get_tag( unsigned char * tag, char tag_id, int * moved );
int write_tags( unsigned char * dst, PyObject * dict );

// region.c
void print_region_info( Region * region );

// world.c
PyTypeObject minecraft_WorldType;
