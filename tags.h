/*
tags.h

Header for tag-related functionality
*/

#ifndef TAG_INFO
#define TAG_INFO

#define TAG_END         0
#define TAG_BYTE        1
#define TAG_SHORT       2
#define TAG_INT         3
#define TAG_LONG        4
#define TAG_FLOAT       5
#define TAG_DOUBLE      6
#define TAG_BYTE_ARRAY  7
#define TAG_STRING      8
#define TAG_LIST        9
#define TAG_COMPOUND    10
#define TAG_INT_ARRAY   11

extern TagType leveldat_tags[];
extern TagType chunk_tags[];

#endif
