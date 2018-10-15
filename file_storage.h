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
    size_t freeBlocksSize;
    size_t nextFreeBlock;
    uint16_t* freeINodes;
    size_t freeINodesSize;
    size_t nextFreeINode;
};

void initFileStorage(const char* fileName);

void tearDownFileStorage();

void createFs(int maxSize);

void ls(const char* directory, size_t maxFileCount, char** dest);

void mkdir(const char* path);

void setFileContents(const char* path, const char* contents);

void cat(const char* path, char* dest);

void rm(const char* path);

void rmdir(const char* path);
