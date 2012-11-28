/*
ntb.c

Decompression and NBT reading utilities
*/

#include <Python.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "minecraft.h"
#include "zlib.h"

// Takes a buffer and prints it prettily
void dump_buffer(unsigned char * buffer, int count)
{
    int i;

    for ( i = 0; i < count; i++)
    {
        if ( i % 16 == 0 )
            printf(" %4i ", i); 

        printf("%2x", buffer[i]);

        if ( i % 4 == 3 )
            printf(" ");
        if ( i % 16 == 15 )
            printf("\n");
    }
    printf("\n");
}

// Inflate x bytes of compressed data, using zlib
int inf( unsigned char * dst, unsigned char * src, int bytes )
{
    int ret;
    z_stream strm;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    strm.next_out = dst;
    strm.avail_out = CHUNK_INFLATE_MAX;
    strm.next_in = src;
    strm.avail_in = bytes;

    inflateInit2(&strm, MAX_WBITS + 32); // + 32 bits for header detection and gzip
    ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);
    
    if ( ret != Z_STREAM_END )
    {
        PyErr_Format(PyExc_Exception, "Unable to decompress (RC: %d | Error: %s)", ret, strm.msg);
    }

    return ret;
}

// Takes a region file stream and a chunk location and finds and decompresses
// the chunk to the passed buffer
int decompress_chunk( FILE * region_file, unsigned char * chunk_buffer, int chunkX, int chunkZ )
{
    unsigned int offset, chunk_length, compression_type;
    unsigned char small_buffer[5], compressed_chunk_buffer[1048576];

    printf("Finding chunk (%d, %d)\n", chunkX, chunkZ);
    fseek(region_file, 4 * ((chunkX & 31) + (chunkZ & 31) * 32), SEEK_SET);
    fread(small_buffer, 4, 1, region_file);

    offset = bytes_to_long(small_buffer, 3) * 4096;
    if ( offset == 0 )
    {
        //printf("Chunk is empty!\n");
        return 0;
    }

    printf("Offset: %d | Length: %d\n", offset, small_buffer[3] * 4096);

    // Seek to start of chunk, copy it into buffer
    fseek(region_file, offset, SEEK_SET);
    memset(small_buffer, 0, 5);
    fread(small_buffer, 5, 1, region_file);
    chunk_length = bytes_to_long(small_buffer, 4);
    compression_type = bytes_to_long(small_buffer + 4, 1);
    printf("Actual Length: %d\n", chunk_length);
    printf("Compression scheme: %d\n", compression_type);

    fread(compressed_chunk_buffer, chunk_length - 1, 1, region_file);
    inf(chunk_buffer, compressed_chunk_buffer, chunk_length - 1);

    return 1;
}

// For testing
int main( int argc, char *argv[] )
{
    FILE *fp;
    unsigned char * src, * dst;

    if ( argc < 2 )
    {
        printf("No region filename specified");
        exit(1);
    }

    /*

    TEST CODE

    */

    src = calloc(10000, 1);
    dst = calloc(10000, 1);

    fp = fopen(argv[1], "rb"); // TODO: Does 'rb' vs 'r' mean anything?
    if( fp != NULL )
    {
        struct stat stbuf;
        int size;

        fstat(fileno(fp), &stbuf);
        size = stbuf.st_size;
        printf("Size is: %d\n", size);

        fread(src, 1, size, fp);
        dump_buffer(src, 600);

        inf(dst, src, size);

        dump_buffer(dst, 600);
    }

    free(src);
    free(dst);

    /*

    END OF TEST CODE

    */

    fclose(fp);

    return 0;
}
