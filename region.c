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
#include "tags.h"

void print_region_info( Region *region )
{
    printf("Region | X: %d Z: %d\nCurrent Size: %d | Buffer Size: %d\nBuffer: %p\nNext: %p\n", region->x, region->z, region->current_size, region->buffer_size, region->buffer, region->next);
}

// Takes a region buffer, and updates it with a chunk, with the assumptions
// that a) the chunk belongs in the region buffer and b) the region buffer
// is large enough to handle a previusly-empty chunk being written to the end
int update_region( Region *region, Chunk *chunk )
{
    int i, location, offset, timestamp, last_offset, uncompressed_size, compressed_size, difference, new_sector_count;
    unsigned char sector_count, last_sector_count, *end, *uncompressed_chunk, *compressed_chunk, *chunk_location;

    chunk_location = NULL;

    offset = 4 *((chunk->x & 31) + (chunk->z & 31) *32);
    location = swap_endianness(region->buffer + offset, 3);
    sector_count = *(region->buffer + offset + 3);
    timestamp = swap_endianness(region->buffer + offset + 4096, 4);

    // TODO: Update timestamp, if desired

    // Find the last offset in the file, both so we can know the end if we
    // need to move memory to insert extra chunk sectors, and so we can append
    // to the end if the chunk was previously empty
    last_offset = 2;
    last_sector_count = 0;
    for( i = 0; i < 4096; i += 4 )
    {
        int current_offset;
        unsigned char current_sector_count;

        current_offset = swap_endianness(region->buffer + i, 3);
        if( current_offset > last_offset )
        {
            last_offset = current_offset;
            last_sector_count = current_sector_count;
        }
    }
    printf("Last Offset: %d | Sector Count: %d\n", last_offset, last_sector_count);

    // Write out the chunk to a temporary buffer, as a staging ground for
    // compression directly to the region_buffer
    uncompressed_chunk = malloc(1000000);
    uncompressed_size = write_tags(uncompressed_chunk, chunk->dict, chunk_tags);
    printf("Chunk (size %d) written to intermediate buffer!\n", uncompressed_size);

    // Compress the chunk, so we know the exact size the chunk will take up in
    // memory and can adjust the buffer size accordingly
    compressed_chunk = malloc(100000);
    compressed_size = 0;
    def(compressed_chunk, uncompressed_chunk, uncompressed_size, 0, &compressed_size);
    printf("Chunk compressed (size %d) to second intermediate buffer!\n", compressed_size);
   
    // Ensure that the buffer area we have will be big enough for the 
    // compressed chunk, including header
    new_sector_count = (compressed_size + 4096 + 5 - 1) / 4096; // ceil(A / B) = (A + B - 1) / B
    
    // new_sector_count vs. sector_count
    difference = new_sector_count - sector_count;
    printf("Difference: %d\n", difference);
    if( difference != 0 )
    {
        // Create space, if needed
        if( region->current_size + difference *4096 >= region->buffer_size )
        {
            unsigned char *new_region_buffer;

            printf("Buffer is too small, increasing size!\n");
            // Allocate a new region, with a few extra sectors worth of padding
            new_region_buffer = malloc(region->current_size + (difference + 4) *4096);

            // TODO: Optimize, since if the chunk information is being
            // inserted, chunks after it will be moved in memory again
            memcpy(new_region_buffer, region->buffer, region->current_size);
            
            free(region->buffer);
            region->buffer = new_region_buffer;
        }

        // Shift chunks after this chunk after if needed
        if( location != 0 )
        {
            void *next_chunk, *next_chunk_after;
            int num;

            num = (last_offset + last_sector_count - (location + sector_count)) *4096;
            next_chunk = region->buffer + (location + sector_count) *4096;
            next_chunk_after = region->buffer + (location + sector_count + difference) *4096;
            printf("Shifting %d bytes worth of chunk data from %p to %p\n", num, next_chunk, next_chunk_after);

            memmove(next_chunk_after, next_chunk, num);

            // Adjust indices
            for( i = 0; i < 4096; i += 4 )
            {
                int o;

                o = swap_endianness(region->buffer + i, 3);
                if( o > location )
                {
                    o += difference;
                    memcpy(region->buffer + i, &o, 3);
                    swap_endianness_in_memory(region->buffer + i, 3);
                }
            }
        }
    }

    if( location == 0 && sector_count == 0 )
    {
        printf("Chunk was previously empty, appending to end of buffer");
        location = last_offset + last_sector_count;    
    }

    *(int *) (region->buffer + location *4096) = compressed_size + 1;
    swap_endianness_in_memory(region->buffer + location *4096, 4);
    *(unsigned char *) (region->buffer + location *4096 + 4) = 2; // Compression type
    memcpy(region->buffer + location *4096 + 5, compressed_chunk, compressed_size);

    free(compressed_chunk);
    free(uncompressed_chunk);

    return 0;
}

/*
Save the region to file
  *region - region information to save
  path    - path to directory that should contain region file
*/
int save_region( Region *region, char *path )
{
    FILE *fp;
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
int unload_region( Region *region, char *path )
{
    int rc;

    rc = save_region(region, path);
    if( region->next != NULL)
        unload_region(region->next, path); // Could potentially loop
    free(region->buffer);
    free(region);

    return rc;
}
