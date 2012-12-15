/*
minecraft.h

Extension module header file
*/

static int SMALL_DEFLATE_MAX = 10000;
static int SMALL_INFLATE_MAX = 20000;
static int CHUNK_DEFLATE_MAX = 65536;
static int CHUNK_INFLATE_MAX = 131072;

// structs
typedef struct TagType{
    char *name;
    int id;
    bool empty_byte_list;
    unsigned char sub_tag_id; // Optional, for List tag
} TagType;

typedef struct {
    PyObject_HEAD
    unsigned short id;
    unsigned char data, blocklight, skylight;
} Block;

typedef struct {
    PyObject_HEAD
    PyObject *world, *dict;
    int x, z;
} Chunk;

typedef struct {
    unsigned char *buffer;
    int x, z, buffer_size, current_size;
    struct Region *next; // Meant to function as a linked list, as part of the World
} Region;

typedef struct {
    PyObject_HEAD
    PyObject *level; // level.dat dictionary
    char *path;      // path to the world
    Region *regions;

    // Chunk holding code, probably will be moved
    PyObject *chunks[100];
    int chunk_count;
} World;

// block.c
PyTypeObject minecraft_BlockType;
int Block_init( Block *self, PyObject *args, PyObject *kwds );

// chunk.c
PyTypeObject minecraft_ChunkType;
int Chunk_init( Chunk *self, PyObject *args, PyObject *kwds );
PyObject *Chunk_get_block( Chunk *self, PyObject *args );
PyObject *Chunk_put_block( Chunk *self, PyObject *args );

// nbt.c
long swap_endianness( unsigned char *buffer, int bytes );
void swap_endianness_in_memory( unsigned char *buffer, int bytes );
void dump_buffer( unsigned char *buffer, int count );
int inf( unsigned char *dst, unsigned char *src, int bytes, int mode );
int def( unsigned char *dst, unsigned char *src, int bytes, int mode, int *size );
PyObject *get_tag( unsigned char *tag, char tag_id, int *moved );
int write_tags( unsigned char *dst, PyObject *dict, TagType tags[] );

// region.c
void print_region_info( Region *region );

// world.c
PyTypeObject minecraft_WorldType;
Region *load_region( World *self, int x, int z );
