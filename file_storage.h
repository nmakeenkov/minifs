#pragma once

#include "structs.h"

#include <stdint.h>
#include <stdio.h>

struct FileStorage
{
    FILE* fileRead;
    FILE* fileWrite;
    struct SuperBlock superBlock;
    uint16_t* freeBlocks;
    uint16_t* freeINodes;
};

struct FileReader
{
    struct INode* iNode;
    uint32_t pos;
};

void initFileStorage();

void tearDownFileStorage();

void createFs(int maxSize);

void ls(char* directory, int maxFileCount, char** dest);

void mkdir(char* path);

size_t readFromFile(struct FileReader* fileReader, void* dest, size_t size);
