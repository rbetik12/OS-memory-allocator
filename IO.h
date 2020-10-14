#ifndef LAB1_IO_H
#define LAB1_IO_H

#include <stdlib.h>
#include <stdio.h>
#include <zconf.h>
#include <semaphore.h>

struct WriteToMemoryArgs {
    int randomFD;
    unsigned char* memoryRegion;
    int mBytes;
    int start;
    int end;
    pthread_t threadId;
};

struct ReadFromFileArgs {
    FILE* file;
    sem_t sem;
    size_t fileIndex;
};

struct WriteToFilesArgs {
    size_t filesAmount;
    size_t fileSizeQuotient;
    size_t fileSizeRemainder;
    unsigned char* memoryRegion;
    FILE ** files;
    sem_t* fileSems;
};

void CleanFile(int fd);

void CleanFiles(size_t filesAmount, FILE** files);

void OpenFiles(size_t filesAmount, FILE** files);

void OpenFile(int fd);

unsigned char ReadChar(int randomFD);

void* ReadFile(void* args);

void* WriteToMemory(void* args);

void WriteToFile(const unsigned char* memoryRegion, FILE* file, size_t fileNum, size_t bytesCount);

void* WriteToFilesOnce(void* args);

//_Noreturn void* WriteToFiles(void * args);

#endif //LAB1_IO_H
