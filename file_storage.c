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

struct FileReader
{
    struct INode* iNode;
    uint32_t pos;
};

void initFileStorage(const char* fileName)
{
    // Should be static_assert, but there are only hacky(IMHO) ways to do that in C
    assert(sizeof(struct SuperBlock) < BLOCK_SIZE);

    fileStorage = malloc(sizeof(*fileStorage));
    fileStorage->fileRead = fopen(fileName, "rb");
    fileStorage->fileWrite = fopen(fileName, "wb");
}

void tearDownFileStorage()
{
    fclose(fileStorage->fileRead);
    fclose(fileStorage->fileWrite);
    free(fileStorage->freeBlocks);
    free(fileStorage->freeINodes);
    free(fileStorage);
}

void createFs(int maxSize)
{
    fseek(fileStorage->fileWrite, 0, SEEK_SET);
    fileStorage->superBlock.blocksCount = (uint16_t) (maxSize / BLOCK_SIZE);
    fileStorage->superBlock.sizeOfINode = sizeof(struct INode);
    fileStorage->superBlock.magicNumber = MAGIC_NUMBER;


    fileStorage->freeBlocksSize = (size_t) fileStorage->superBlock.blocksCount - 2;
    fileStorage->freeBlocks = malloc(sizeof(uint16_t) * fileStorage->freeBlocksSize);
    fileStorage->nextFreeBlock = 0;
    for (uint16_t i = 0; i < fileStorage->freeBlocksSize; ++i)
    {
        fileStorage->freeBlocks[i] = i + 3;
    }
    fileStorage->freeINodesSize = BLOCK_SIZE / sizeof(uint16_t) - 1;
    fileStorage->freeINodes = malloc(sizeof(uint16_t) * fileStorage->freeINodesSize);
    fileStorage->nextFreeINode = 0;
    for (uint16_t i = 0; i < fileStorage->freeINodesSize; ++i)
    {
        fileStorage->freeINodes[i] = i + 1;
    }

    assert(fwrite(&fileStorage->superBlock, sizeof(struct SuperBlock), 1, fileStorage->fileWrite) == 1);
    size_t zerosSize = BLOCK_SIZE - sizeof(struct SuperBlock);
    void* zeros = malloc(BLOCK_SIZE);
    memset(zeros, 0, BLOCK_SIZE);
    assert(fwrite(zeros, zerosSize, 1, fileStorage->fileWrite) == 1);

    // Block with i-nodes
    struct INode rootDirectoryINode;
    rootDirectoryINode.type = DIRECTORY_;
    rootDirectoryINode.size = 2;
    rootDirectoryINode.linkCounter = 1;
    memset(rootDirectoryINode.blocks, 0, BLOCKS_COUNT);
    rootDirectoryINode.blocks[0] = 2;
    assert(fwrite(&rootDirectoryINode, sizeof(rootDirectoryINode), 1, fileStorage->fileWrite) == 1);

    zerosSize = BLOCK_SIZE - sizeof(rootDirectoryINode);
    assert(fwrite(zeros, zerosSize, 1, fileStorage->fileWrite) == 1);

    for (size_t i = 2; i < fileStorage->superBlock.blocksCount; ++i)
    {
        assert(fwrite(zeros, 1, BLOCK_SIZE, fileStorage->fileWrite) == BLOCK_SIZE);
    }
    free(zeros);
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

void freeINode(uint16_t iNodeId)
{
    fileStorage->freeINodes[--fileStorage->nextFreeINode] = iNodeId;
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
    if (fileStorage->nextFreeBlock == fileStorage->freeBlocksSize)
    {
        raise(SIGUSR1);
    }
    uint16_t idx = fileStorage->freeBlocks[fileStorage->nextFreeBlock++];
    resetBlock(idx, size, data);
    return idx;
}

void freeBlock(uint16_t blockId)
{
    fileStorage->freeBlocks[--fileStorage->nextFreeBlock] = blockId;
}

void resetINode(uint16_t id, struct INode* iNode, const void* newData)
{
    size_t size = iNode->size;
    size_t i = 0;
    bool isBlockUsed = true;
    while (size > 0)
    {
        if (i == BLOCKS_COUNT)
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

uint16_t createNewINode(struct INode* iNode, const void* data)
{
    if (fileStorage->nextFreeINode == fileStorage->freeINodesSize)
    {
        raise(SIGUSR1);
    }
    uint16_t idx = fileStorage->freeINodes[fileStorage->nextFreeINode++];
    iNode->linkCounter = 1;
    iNode->blocks[0] = 0;
    resetINode(idx, iNode, data);
    return idx;
}

size_t readFromFile(struct FileReader* fileReader, void* dest, size_t size)
{
    if (fileReader->pos + size > fileReader->iNode->size)
    {
        size = fileReader->iNode->size - fileReader->pos;
    }
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

uint16_t getDirectoryNode(const char* directory, bool create, struct INode* iNode)
{
    uint16_t iNodeId = 0;
    *iNode = getINode(iNodeId);

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
            fileReader.iNode = iNode;
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
                    newNode.type = DIRECTORY_;
                    newNode.size = sizeof(zero);
                    newNodeId = createNewINode(&newNode, &zero);
                    void* buff = malloc(sizeof(uint16_t) + sizeof(struct FileListEntry) * (len + 1));
                    fileReader.pos = sizeof(uint16_t);
                    readFromFile(&fileReader, buff + sizeof(uint16_t), sizeof(struct FileListEntry) * len);
                    struct FileListEntry newEntry;
                    newEntry.iNodeId = newNodeId;
                    strcpy(newEntry.name, nextName);
                    memcpy(buff + sizeof(uint16_t) + sizeof(struct FileListEntry) * len, &newEntry, sizeof(struct FileListEntry));
                    ++len;
                    iNode->size += sizeof(struct FileListEntry);
                    memcpy(buff, &len, sizeof(len));
                    resetINode(iNodeId, iNode, buff);
                    free(buff);
                }
                else
                {
                    raise(SIGUSR1);
                }
            }
            iNodeId = newNodeId;
            *iNode = getINode(newNodeId);
            if (iNode->type != DIRECTORY_)
            {
                raise(SIGUSR1);
            }
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
    return iNodeId;
}

void ls(const char* directory, size_t maxFileCount, char** dest)
{
    struct FileReader fileReader;
    struct INode iNode;
    getDirectoryNode(directory, false, &iNode);
    fileReader.iNode = &iNode;
    fileReader.pos = 0;

    uint16_t len;
    assert(readFromFile(&fileReader, &len, sizeof(len)) == sizeof(len));
    struct FileListEntry fileListEntry;
    size_t i;
    for (i = 0; len > 0 && i < maxFileCount; --len, ++i)
    {
        assert(readFromFile(&fileReader, &fileListEntry, sizeof(fileListEntry)) == sizeof(fileListEntry));
        strcpy(dest[i], fileListEntry.name);
    }
    dest[i][0] = '\0';
}

void mkdir(const char* path)
{
    struct INode iNode;
    getDirectoryNode(path, true, &iNode);
}

uint16_t getFileNodeId(const char* path, const char* contents)
{
    size_t pathLength = strlen(path);
    size_t i = pathLength;
    while (--i > 0 && path[i] != '/');
    char* directory = malloc(sizeof(char) * (i + 2));
    memcpy(directory, path, sizeof(char) * (i + 1));
    directory[i + 1] = '\0';
    struct INode iNode;
    uint16_t iNodeId = getDirectoryNode(directory, contents != NULL, &iNode);
    free(directory);
    char* name = malloc(sizeof(char) * (pathLength - i));
    memcpy(name, path + i + 1, pathLength - i - 1);
    name[pathLength - i - 1] = '\0';

    struct FileReader fileReader;
    fileReader.iNode = &iNode;
    fileReader.pos = 0;

    uint16_t len;
    assert(readFromFile(&fileReader, &len, sizeof(len)) == sizeof(len));
    uint16_t fileNodeId = 0;
    struct FileListEntry fileListEntry;
    for (uint16_t t = 0; t < len; ++t)
    {
        assert(readFromFile(&fileReader, &fileListEntry, sizeof(fileListEntry)) == sizeof(fileListEntry));
        if (strcmp(fileListEntry.name, name) == 0)
        {
            fileNodeId = fileListEntry.iNodeId;
        }
    }
    if (fileNodeId == 0)
    {
        if (contents == NULL)
        {
            raise(SIGUSR1);
        }
        void* buff = malloc(sizeof(uint16_t) + (len + 1) * sizeof(struct FileListEntry));
        ++len;
        memcpy(buff, &len, sizeof(uint16_t));
        fileReader.pos = sizeof(uint16_t);
        readFromFile(&fileReader, buff + sizeof(uint16_t), (len - 1) * sizeof(struct FileListEntry));
        strcpy(fileListEntry.name, name);
        struct INode newINode;
        newINode.type = FILE_;
        newINode.size = strlen(contents) + 1; // null-termination
        fileListEntry.iNodeId = createNewINode(&newINode, contents);
        memcpy(buff + sizeof(uint16_t) + (len - 1) * sizeof(struct FileListEntry), &fileListEntry, sizeof(fileListEntry));
        iNode.size += sizeof(struct FileListEntry);
        resetINode(iNodeId, &iNode, buff);
        free(buff);
    }
    else if (contents != NULL)
    {
        struct INode newINode = getINode(fileNodeId);
        if (newINode.type != FILE_)
        {
            raise(SIGUSR1);
        }
        newINode.size = strlen(contents) + 1; // null-termination
        resetINode(fileNodeId, &newINode, contents);
    }
    free(name);

    return fileNodeId;
}

void setFileContents(const char* path, const char* contents)
{
    getFileNodeId(path, contents);
}

void cat(const char* path, char* dest)
{
    uint16_t fileNodeId = getFileNodeId(path, NULL);
    struct INode iNode = getINode(fileNodeId);
    struct FileReader fileReader;
    fileReader.iNode = &iNode;
    fileReader.pos = 0;
    readFromFile(&fileReader, dest, fileReader.iNode->size);
}

void rmImpl(const char* path, Type type)
{
    size_t pathLength = strlen(path);
    size_t i = pathLength;
    while (--i > 0 && path[i] != '/');
    char* directory = malloc(sizeof(char) * (i + 2));
    memcpy(directory, path, sizeof(char) * (i + 1));
    directory[i + 1] = '\0';
    struct INode iNode;
    uint16_t iNodeId = getDirectoryNode(directory, false, &iNode);
    free(directory);
    char* name = malloc(sizeof(char) * (pathLength - i));
    memcpy(name, path + i + 1, pathLength - i - 1);
    name[pathLength - i - 1] = '\0';

    struct FileReader fileReader;
    fileReader.iNode = &iNode;
    fileReader.pos = 0;

    uint16_t len;
    assert(readFromFile(&fileReader, &len, sizeof(len)) == sizeof(len));
    uint16_t fileNodeId = 0;
    struct FileListEntry fileListEntry;
    for (uint16_t t = 0; t < len; ++t)
    {
        assert(readFromFile(&fileReader, &fileListEntry, sizeof(fileListEntry)) == sizeof(fileListEntry));
        if (strcmp(fileListEntry.name, name) == 0)
        {
            fileNodeId = fileListEntry.iNodeId;
        }
    }

    if (fileNodeId == 0)
    {
        raise(SIGUSR1);
    }
    else
    {
        struct INode iNodeToRemove = getINode(fileNodeId);
        if (iNodeToRemove.type != type)
        {
            raise(SIGUSR1);
        }
        // If removing directory, it's description should only contain 0 - the count of entries
        if (iNodeToRemove.type == DIRECTORY_ && iNodeToRemove.size != sizeof(uint16_t))
        {
            raise(SIGUSR1);
        }
        for (size_t blockNum = 0; blockNum * BLOCK_SIZE < iNodeToRemove.size; ++blockNum)
        {
            freeBlock(iNodeToRemove.blocks[blockNum]);
        }
        freeINode(fileNodeId);

        void* buff = malloc(sizeof(uint16_t) + len * sizeof(struct FileListEntry));
        --len;
        memcpy(buff, &len, sizeof(uint16_t));
        void* buffPtr = buff + sizeof(uint16_t);
        fileReader.pos = sizeof(uint16_t);
        for (size_t i = 0; i <= len; ++i)
        {
            readFromFile(&fileReader, &fileListEntry, sizeof(struct FileListEntry));
            if (strcmp(fileListEntry.name, name) != 0)
            {
                memcpy(buffPtr, &fileListEntry, sizeof(struct FileListEntry));
                buffPtr += sizeof(struct FileListEntry);
            }
        }
        iNode.size -= sizeof(struct FileListEntry);
        resetINode(iNodeId, &iNode, buff);
        free(buff);
    }
    free(name);
}

void rm(const char* path)
{
    rmImpl(path, FILE_);
}

void rmdir(const char* path)
{
    if (strcmp(path, "/") == 0)
    {
        raise(SIGUSR1);
    }
    rmImpl(path, DIRECTORY_);
}
