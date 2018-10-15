#pragma once
#include <stdint.h>

#define NAME_MAX_LENGTH 14
#define BLOCKS_COUNT 12

enum TypeEnum
{
    EMPTY = 0,
    DIRECTORY_ = 1,
    FILE_ = 2
};

typedef uint16_t Type;

#pragma pack(push, 1)

struct INode
{
    Type type;
    uint32_t size;
    uint16_t linkCounter;
    // TODO(nmakeenkov): Also add indirect blocks
    uint16_t blocks[BLOCKS_COUNT];
};

struct FileListEntry
{
    uint16_t iNodeId;
    char name[NAME_MAX_LENGTH];
};

struct SuperBlock
{
    uint16_t sizeOfINode;
    uint16_t blocksCount;
    int32_t magicNumber;
};

#pragma pack(pop)
