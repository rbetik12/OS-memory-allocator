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
};

void CleanFile(int fd);

void CleanFiles(size_t filesAmount, FILE** files);

void OpenFiles(size_t filesAmount, FILE** files);

unsigned char ReadChar(int randomFD);

void* ReadFile(void* args);

void* WriteToMemory(void* args);

void WriteToFile(const unsigned char* memoryRegion, FILE* file, size_t fileNum, size_t bytesCount);

#endif //LAB1_IO_H
