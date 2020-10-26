#ifndef LAB1_IO_H
#define LAB1_IO_H

#include <stdlib.h>
#include <stdio.h>
#include <zconf.h>
#include <semaphore.h>

static sem_t fileSync;

struct WriteToMemoryArgs {
    int randomFD;
    unsigned char* memoryRegion;
    int start;
    int end;
    pthread_t threadId;
};

struct ReadFromFileArgs {
    int fd;
};

struct WriteToFilesArgs {
    size_t filesAmount;
    size_t fileSizeQuotient;
    size_t fileSizeRemainder;
    unsigned char* memoryRegion;
    int* files;
};

void CleanFile(int fd);

void CleanFiles(size_t filesAmount, int* files);

void OpenFiles(size_t filesAmount, int* files);

void OpenFile(int fd);

unsigned char ReadChar(int randomFD);

void* ReadFile(void* args);

void* WriteToMemory(void* args);

void WriteToFile(const unsigned char* memoryRegion, int fd, size_t fileNum, size_t bytesCount);

void* WriteToFilesOnce(void* args);

_Noreturn void* WriteToFiles(void * args);

#endif //LAB1_IO_H
