/*
region.c

Functions to writing to region files, mostly to be used by chunks when saving
*/

#include <Python.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "minecraft.h"

void print_region_info( Region * region )
{
    printf("Region | X: %d Z: %d\nCurrent Size: %d | Buffer Size: %d\nBuffer: %p\nNext: %p\n", region->x, region->z, region->current_size, region->buffer_size, region->buffer, region->next);
}

// Takes a region buffer, and updates it with a chunk, with the assumptions
// that a) the chunk belongs in the region buffer and b) the region buffer
// is large enough to handle a previusly-empty chunk being written to the end
int update_region( unsigned char * region_buffer, Chunk *chunk )
{
    int i, location, offset, timestamp, last_offset, size;
    unsigned char sector_count, last_sector_count, * end, * uncompressed_chunk;

    offset = 4 * ((chunk->x % 32) + (chunk->z % 32) * 32);
    location = swap_endianness(region_buffer + offset, 3);
    sector_count = *(region_buffer + offset + 3);
    timestamp = swap_endianness(region_buffer + offset + 4096, 4);

    // Find the last offset in the file, both so we can know the end if we
    // need to move memory to insert extra chunk sectors, and so we can append
    // to the end if the chunk was previously empty
    last_offset = 2;
    last_sector_count = 0;
    for( i = 0; i < 4096; i += 4 )
    {
        int current_offset;
        unsigned char current_sector_count;

        current_offset = swap_endianness(region_buffer + i, 3);
        if( current_offset > last_offset )
        {
            last_offset = current_offset;
            last_sector_count = current_sector_count;
        }
    }

    // Write out the chunk to a temporary buffer, as a staging ground for
    // compression directly to the region_buffer
    uncompressed_chunk = malloc(10000);
    size = write_tags(uncompressed_chunk, chunk->dict);
    printf("Chunk (size %d) written to intermediate buffer!\n", size);
    dump_buffer(uncompressed_chunk, 480);

    /*
    TODO: Compress uncompressed_chunk to location in region_buffer, and update
          necessary headers
    // In the case where a chunk was previously empty, we'll append the newly
    // populated chunk to the end of the file
    if( location == 0 && sector_count == 0 )
    {
        unsigned char * chunk_location;
        int actual_size;

        printf("Chunk was previously empty, appending to end of buffer");

        chunk_location = region_buffer + (max_offset + max_sector_count) * 4096;
        actual_size = write_tags(chunk_location + 4, chunk->dict);
        * (int *) location = swap_endianness(&actual_size, 4); // Chunk header

        // TODO: COMPRESS
        location = swap_endianness(region_buffer + offset
        memcpy(region_buffer + offset, &

    }
    else
    {

    }
    */

    free(uncompressed_chunk);

    return 0;
}

/*
Save the region to file
  *region - region information to save
  path    - path to directory that should contain region file
*/
int save_region( Region *region, char * path )
{
    FILE * fp;
    char filename[1000]; // TODO: Dynamic

    sprintf(filename, "%s/region/r.%d.%d.mca", path, region->x, region->z);
    fp = fopen(filename, "wb");
    if( fp == NULL )
    {
        PyErr_Format(PyExc_Exception, "Unable to open %s for writing", filename);
        return -1;
    }

    fwrite(region->buffer, 1, region->current_size, fp); 
    fclose(fp);

    printf("Region saved!\n");

    return 0;
}

/*
Save the region to file, and de-allocate as necessary
  *region - region information to save, and resource to de-allocate
  path    - path to directory that should contain region file
*/
int unload_region( Region *region, char * path )
{
    int rc;

    rc = save_region(region, path);
    free(region->buffer);
    free(region);

    return rc;
}
