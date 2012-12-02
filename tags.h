/*
tags.h

Header for tag-related functionality
*/

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

typedef struct TagType{
    char * name;
    int id;
    bool empty_byte_list;
    unsigned char sub_tag_id; // Optional, for List tag
} TagType;

TagType leveldat_tags[] = {
    {"Data", TAG_COMPOUND},
    {"version", TAG_INT},
    {"initialized", TAG_BYTE},
    {"LevelName", TAG_STRING},
    {"generatorName", TAG_STRING},
    {"generatorVersion", TAG_INT},
    {"generatorOptions", TAG_STRING},
    {"RandomSeed", TAG_LONG},
    {"MapFeatures", TAG_BYTE},
    {"LastPlayed", TAG_LONG},
    {"SizeOnDisk", TAG_LONG},
    {"allowCommands", TAG_BYTE},
    {"hardcore", TAG_BYTE},
    {"GameType", TAG_INT},
    {"Time", TAG_LONG},
    {"DayTime", TAG_LONG},
    {"SpawnX", TAG_INT},
    {"SpawnY", TAG_INT},
    {"SpawnZ", TAG_INT},
    {"raining", TAG_BYTE},
    {"rainTime", TAG_INT},
    {"thundering", TAG_BYTE},
    {"thunderTime", TAG_INT},
    {"Player", TAG_COMPOUND},
    {"GameRules", TAG_COMPOUND},
    {"commandBlockOutput", TAG_STRING},
    {"doFireTick", TAG_STRING},
    {"doMobLoot", TAG_STRING},
    {"doMobSpawning", TAG_STRING},
    {"doTileDrops", TAG_STRING},
    {"keepInventory", TAG_STRING},
    {"mobGriefing", TAG_STRING},
    {NULL, NULL} // Sentinel
};

TagType chunk_tags[] = {
    // Basic chunk tags
    {"Level", TAG_COMPOUND},
    {"xPos", TAG_INT},
    {"zPos", TAG_INT},
    {"LastUpdate", TAG_LONG},
    {"TerrainPopulated", TAG_BYTE},
    {"Biomes", TAG_BYTE_ARRAY},
    {"HeightMap", TAG_INT_ARRAY},
    {"Sections", TAG_LIST},
    {"Y", TAG_BYTE},
    {"Blocks", TAG_BYTE_ARRAY},
    {"Add", TAG_BYTE_ARRAY},
    {"Data", TAG_BYTE_ARRAY},
    {"BlockLight", TAG_BYTE_ARRAY},
    {"SkyLight", TAG_BYTE_ARRAY},
    {"Entities", TAG_LIST, true, TAG_BYTE_ARRAY},
    {"TileEntities", TAG_LIST, true, TAG_BYTE_ARRAY},
    {"TileTicks", TAG_LIST},
    // TODO: Fill in other tags
    {NULL, NULL}
};
