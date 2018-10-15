#include "file_storage.h"

#include "structs.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <memory.h>
#include <signal.h>
#include <stdbool.h>

static const size_t BLOCK_SIZE = 1 << 10;
static const int32_t MAGIC_NUMBER = 1337;

struct FileStorage* fileStorage;

void initFileStorage()
{
    // Should be static_assert, but there are only hacky(IMHO) ways to do that in C
    assert(sizeof(struct SuperBlock) < BLOCK_SIZE);

    fileStorage = malloc(sizeof(*fileStorage));
    fileStorage->fileRead = fopen("kek.bin", "rb");
    fileStorage->fileWrite = fopen("kek.bin", "wb");
}

void tearDownFileStorage()
{
    fclose(fileStorage->fileRead);
    fclose(fileStorage->fileWrite);
    free(fileStorage);
}

void createFs(int maxSize)
{
    fseek(fileStorage->fileWrite, 0, SEEK_SET);
    fileStorage->superBlock.blocksCount = (uint16_t) (maxSize / BLOCK_SIZE);
    fileStorage->superBlock.sizeOfINode = sizeof(struct INode);
    fileStorage->superBlock.magicNumber = MAGIC_NUMBER;

    fileStorage->freeBlocks = malloc(sizeof(uint16_t) * (fileStorage->superBlock.blocksCount - 2));
    for (uint16_t i = 0; i < fileStorage->superBlock.blocksCount - 2; ++i)
    {
        if (i == fileStorage->superBlock.blocksCount - 3)
        {
            fileStorage->freeBlocks[i] = 0;
        }
        else
        {
            fileStorage->freeBlocks[i] = i + 3;
        }
    }
    fileStorage->freeINodes = malloc(BLOCK_SIZE);
    for (uint16_t i = 0; i < BLOCK_SIZE / sizeof(uint16_t); ++i)
    {
        if (i == BLOCK_SIZE / sizeof(uint16_t) - 1)
        {
            fileStorage->freeINodes[i] = 0;
        }
        else
        {
            fileStorage->freeINodes[i] = i + 1;
        }
    }

    assert(fwrite(&fileStorage->superBlock, sizeof(struct SuperBlock), 1, fileStorage->fileWrite) == 1);
    size_t zerosSize = BLOCK_SIZE - sizeof(struct SuperBlock);
    void* zeros = malloc(zerosSize);
    memset(zeros, 0, zerosSize);
    assert(fwrite(zeros, zerosSize, 1, fileStorage->fileWrite) == 1);
    free(zeros);

    // Block with i-nodes
    struct INode rootDirectoryINode;
    rootDirectoryINode.type = DIRECTORY_;
    rootDirectoryINode.size = 2;
    rootDirectoryINode.linkCounter = 1;
    memset(rootDirectoryINode.blocks, 0, 12);
    rootDirectoryINode.blocks[0] = 2;
    assert(fwrite(&rootDirectoryINode, sizeof(rootDirectoryINode), 1, fileStorage->fileWrite) == 1);
    fprintf(stderr, "?1\n");

    zerosSize = BLOCK_SIZE - sizeof(rootDirectoryINode);
    zeros = malloc(zerosSize);
    memset(zeros, 0, zerosSize);
    assert(fwrite(zeros, zerosSize, 1, fileStorage->fileWrite) == 1);
    free(zeros);
    fprintf(stderr, "?2\n");

    void* block = malloc(BLOCK_SIZE);
    memset(block, 0, BLOCK_SIZE);
    for (size_t i = 2; i < fileStorage->superBlock.blocksCount; ++i)
    {
        assert(fwrite(block, 1, BLOCK_SIZE, fileStorage->fileWrite) == BLOCK_SIZE);
    }
    fprintf(stderr, "?3\n");
    free(block);
    fflush(fileStorage->fileWrite);
}

struct INode getINode(uint16_t id)
{
    struct INode iNode;
    fflush(fileStorage->fileRead);
    fseek(fileStorage->fileRead, BLOCK_SIZE + id * sizeof(struct INode), SEEK_SET);
    assert(fread(&iNode, sizeof(iNode), 1, fileStorage->fileRead) == 1);
    fprintf(stderr, "node %d:: %d %d %d\n", id, iNode.type, iNode.size, iNode.linkCounter);
    return iNode;
}

void setINode(uint16_t id, const struct INode* iNode)
{
    fseek(fileStorage->fileWrite, BLOCK_SIZE + id * sizeof(struct INode), SEEK_SET);
    assert(fwrite(iNode, sizeof(struct INode), 1, fileStorage->fileWrite) == 1);
    fflush(fileStorage->fileWrite);
}

void resetBlock(uint16_t id, size_t size, const void* data)
{
    assert(size <= BLOCK_SIZE);
    fseek(fileStorage->fileWrite, BLOCK_SIZE * id, SEEK_SET);
    assert(fwrite(data, size, 1, fileStorage->fileWrite) == 1);
    fflush(fileStorage->fileWrite);
}

uint16_t createNewBlock(size_t size, const void* data)
{
    uint16_t idx = fileStorage->freeBlocks[0];
    ++fileStorage->freeBlocks;
    resetBlock(idx, size, data);
    return idx;
}


void resetINode(uint16_t id, struct INode* iNode, const void* newData)
{
    size_t size = iNode->size;
    size_t i = 0;
    bool isBlockUsed = true;
    while (size > 0)
    {
        if (i == 11)
        {
            raise(SIGUSR1);
        }
        if (iNode->blocks[i] == 0)
        {
            isBlockUsed = false;
        }
        size_t sizeToWrite = size < BLOCK_SIZE ? size : BLOCK_SIZE;
        if (isBlockUsed)
        {
            resetBlock(iNode->blocks[i++], sizeToWrite, newData);
        }
        else
        {
            iNode->blocks[i++] = createNewBlock(sizeToWrite, newData);
        }
        newData += sizeToWrite;
        size -= sizeToWrite;
    }
    iNode->blocks[i] = 0;
    setINode(id, iNode);
}

uint16_t createNewINode(Type type, uint32_t size, const void* data, struct INode* iNode)
{
    uint16_t idx = fileStorage->freeINodes[0];
    ++fileStorage->freeINodes;
    iNode->type = type;
    iNode->size = size;
    iNode->linkCounter = 1;
    iNode->blocks[0] = 0;
    resetINode(idx, iNode, data);
    return idx;
}

struct INode getDirectoryNode(const char* directory, bool create)
{
    uint16_t iNodeId = 0;
    struct INode iNode = getINode(iNodeId);

    char nextName[NAME_MAX_LENGTH];
    int j = 0;
    if (directory[0] != '/')
    {
        raise(SIGUSR1);
    }
    for (size_t i = 1; i < strlen(directory) || j > 0; ++i)
    {
        if (i == strlen(directory) || directory[i] == '/')
        {
            if (j == 0)
            {
                raise(SIGUSR1);
            }
            nextName[j] = '\0';
            j = 0;

            struct FileReader fileReader;
            fileReader.iNode = &iNode;
            fileReader.pos = 0;

            uint16_t len;
            assert(readFromFile(&fileReader, &len, sizeof(len)) == sizeof(len));
            uint16_t newNodeId = 0;
            struct FileListEntry fileListEntry;
            for (uint16_t t = 0; t < len; ++t)
            {
                assert(readFromFile(&fileReader, &fileListEntry, sizeof(fileListEntry)) == sizeof(fileListEntry));
                if (strcmp(fileListEntry.name, nextName) == 0)
                {
                    newNodeId = fileListEntry.iNodeId;
                }
            }
            if (newNodeId == 0)
            {
                if (create)
                {
                    uint16_t zero = 0;
                    struct INode newNode;
                    newNodeId = createNewINode(DIRECTORY_, sizeof(zero), &zero, &newNode);
                    uint8_t* buff = malloc(sizeof(uint16_t) + sizeof(struct FileListEntry) * (len + 1));
                    fileReader.pos = sizeof(uint16_t);
                    readFromFile(&fileReader, buff, sizeof(struct FileListEntry) * len);
                    struct FileListEntry newEntry;
                    newEntry.iNodeId = newNodeId;
                    strcpy(newEntry.name, nextName);
                    memcpy(buff + sizeof(uint16_t) + sizeof(struct FileListEntry) * len, &newEntry, sizeof(struct FileListEntry));
                    ++len;
                    iNode.size += sizeof(struct FileListEntry);
                    memcpy(buff, &len, sizeof(len));
                    resetINode(iNodeId, &iNode, buff);
                    free(buff);
                }
                else
                {
                    raise(SIGUSR1);
                }
            }
            iNodeId = newNodeId;
            iNode = getINode(newNodeId);
        }
        else
        {
            if (j == NAME_MAX_LENGTH - 1)
            {
                raise(SIGUSR1);
            }
            nextName[j++] = directory[i];
        }
    }
    return iNode;
}

void ls(char* directory, int maxFileCount, char** dest)
{
    struct FileReader fileReader;
    struct INode iNode = getDirectoryNode(directory, false);
    fileReader.iNode = &iNode;
    fileReader.pos = 0;

    uint16_t len;
    assert(readFromFile(&fileReader, &len, sizeof(len)) == sizeof(len));
    struct FileListEntry fileListEntry;
    for (int i = 0; len > 0 && i < maxFileCount; --len, ++i)
    {
        assert(readFromFile(&fileReader, &fileListEntry, sizeof(fileListEntry)) == sizeof(fileListEntry));
        strcpy(dest[i], fileListEntry.name);
    }
}

void mkdir(char* path)
{
    getDirectoryNode(path, true);
}

size_t readFromFile(struct FileReader* fileReader, void* dest, size_t size)
{
    assert(fileReader->pos + size <= fileReader->iNode->size);
    size_t result = 0;
    while (size > 0)
    {
        size_t blockNum = fileReader->pos / BLOCK_SIZE;
        size_t start = fileReader->pos - blockNum * BLOCK_SIZE;
        size_t bytesToRead = size;
        // In different blocks
        if (fileReader->pos / BLOCK_SIZE < (fileReader->pos + size) / BLOCK_SIZE)
        {
            bytesToRead = BLOCK_SIZE - start;
        }
        fflush(fileStorage->fileRead);
        fseek(fileStorage->fileRead, fileReader->iNode->blocks[blockNum] * BLOCK_SIZE + start, SEEK_SET);
        result += fread(dest, 1, bytesToRead, fileStorage->fileRead);
        dest += bytesToRead;
        fileReader->pos += bytesToRead;
        size -= bytesToRead;
    }

    return result;
}
