#include <regex.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

/*

Functionality
---------------------

region = load_region(x, z)
chunk = load_chunk(cx, cz)

*/

int CHUNK_DEFLATE_MAX = 65536;
int CHUNK_INFLATE_MAX = 131072;

// Takes a number of bytes (in big-endian order), starting at a given location,
// and returns the integer they represent
int bytes_to_int( unsigned char * buffer, int bytes )
{
    int transform;

    transform = 0;
    for ( int i = 0; i < bytes; i++ )
    {
        transform = buffer[i]<<((bytes - i - 1) * 8) | transform;
    }
    return transform;
}

// Inflate x bytes of compressed data, using zlib
int inf( char * dst, char * src, int bytes )
{
    int ret;
    unsigned have;
    z_stream strm;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    strm.next_out = dst;
    strm.avail_out = CHUNK_INFLATE_MAX;
    strm.next_in = src;
    strm.avail_in = bytes;

    inflateInit(&strm);
    ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);
    if ( ret != Z_STREAM_END)
    {
        printf("RC: %d | Error: %s\n", ret, strm.msg);
    }

    return ret;
}

// Takes a buffer and prints it prettily
void dump_buffer(unsigned char * buffer, int count)
{
    for ( int i = 0; i < count; i++)
    {
        printf("%2x", buffer[i]);

        if ( i % 4 == 3 )
            printf(" ");
        if ( i % 16 == 15 )
            printf("\n");
    }
}

// Takes a region file stream and a chunk location and finds and decompresses
// the chunk to the passed buffer
int get_chunk( FILE * region_file, unsigned char * chunk_buffer, int chunkX, int chunkZ )
{
    unsigned int offset, chunk_length, compression_type, rc;
    unsigned char small_buffer[5], compressed_chunk_buffer[1048576];

    printf("Finding chunk (%d, %d)\n", chunkX, chunkZ);
    fseek(region_file, 4 * ((chunkX & 31) + (chunkZ & 31) * 32), SEEK_SET);
    fread(small_buffer, 4, 1, region_file);

    offset = bytes_to_int(small_buffer, 3) * 4096;
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
    chunk_length = bytes_to_int(small_buffer, 4);
    compression_type = bytes_to_int(small_buffer + 4, 1);
    printf("Actual Length: %d\n", chunk_length);
    printf("Compression scheme: %d\n", compression_type);

    fread(compressed_chunk_buffer, chunk_length - 1, 1, region_file);
    rc = inf(chunk_buffer, compressed_chunk_buffer, chunk_length - 1);

    return 1;
}

int read_tags( char * dict, unsigned char * tag, int depth )
{
    unsigned char * payload;
    char tag_id;
    short int tag_name_length;

    tag_id = tag[0];
    tag_name_length = bytes_to_int(tag + 1, 2);
    printf("Tag ID: %d | Name Length: %hd\n", tag_id, tag_name_length);

    // Print tag name, if one exists (or just save it)
    if ( tag_name_length > 0 )
    {
        dict = "\'%.*s\':", tag_name_length, tag + 3;
        dict += tag_name_length + 3; // Move ahead to where the data belongs
        printf("Name: %.*s\n", tag_name_length, tag + 3);
    }

    // Move to the payload portion of the tag
    tag += 3 + tag_name_length;
    switch ( tag_id )
    {
        case 0:
            printf("End tag");
            break;

        case 1:
            printf("Byte: %2x", *tag);
            break;

        case 2:
            printf("Short: %4x", *tag);
            break;

        case 3:
            printf("Int: %d", *tag);
            break;

        case 4:
            printf("Long: %16x", *tag);
            break;

        case 5:
            printf("Float: %f", * (double *) tag);
            break;

        case 6:
            printf("Double: %16x", *tag);
            break;

        // Interesting tag cases
        case 7:  // Byte array
            printf("Length: %d (Byte array)", *tag);
            break;

        case 8:  // String
            printf("Length: %2x | String: %.*s", *tag, *tag, tag + 2);
            break;

        // 1 - tagID | 4 - size | size * tagIDs' payloads 
        case 9:  // List
            printf("Tag ID: %2x | Size: %d | Extra", *tag, tag[1]);
            break;

        case 10: // Compound
            strncpy(dict, "{", 1);
            dict += 1;
            printf("COMPOUND TAG, GOING DEEPER!\n");
            strncpy(dict, "}", 1);
            break;
            
        case 11: // Int array
            printf("Length: %d (Int array)", *tag); 
            break;

        default:
            printf("DEFAULT TAG CATCHER");
            break;
    }

    printf("\n");
}

// Convert a chunk to python dictionary format
void chunk_to_dict( unsigned char * chunk_buffer, unsigned char * dict )
{
    /*
    root {
          'Level': {
                    'xPos': 1, 'zPos': 2, ...
    */

    printf("%x\n", dict);
    read_tags(dict, chunk_buffer, 0);
    printf("Now: %x | Dict: %s", dict, dict);
}

int read_chunk( unsigned char * chunk_buffer )
{
    unsigned char dict[100000]; // TODO: Make larger
    
    chunk_to_dict(chunk_buffer, dict); // just read the first tag for now

    return 0;
}

// Get information from the filename
void region_information( int *coords, char *filename )
{
    regmatch_t matches[3];
    regex_t regex;
    int reti;

    coords[0] = -1;
    coords[1] = -1;

    reti = regcomp(&regex, "^.*\\.([^.]*)\\.([^.]*)\\.mca$", REG_EXTENDED);
    if ( reti )
    {
        printf("Could not compile expression");
        return;
    }

    reti = regexec(&regex, filename, 3, matches, 0);
    if ( !reti )
    {
        char coord[20];

        strncpy ( coord, filename + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_eo + 1 );
        coords[0] = atoi(coord);

        strncpy ( coord, filename + matches[2].rm_so, matches[2].rm_eo - matches[2].rm_eo + 1 );
        coords[1]= atoi(coord);

        printf("\nx: %d | z: %d\n", coords[0], coords[1]);
    } 
    else
    {
        printf("NO MATCH!");
    }
}


int main( int argc, char *argv[] )
{
    FILE *fp;
    unsigned char chunk[1024000];
    int coords[2];

    if ( argc < 2 )
    {
        printf("No region filename specified");
        exit(1);
    }

    // Find chunk ranges the file contains
    region_information(coords, argv[1]);

    // Chunk locations
    printf("Opening %s\n---------\n\n", argv[1]);

    if ( fp = fopen(argv[1], "r") )
    {
        get_chunk(fp, chunk, 0, 5);
        read_chunk(chunk);
    }

    fclose(fp);
}

