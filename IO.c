#include "IO.h"
#include "Constants.h"
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

//sem_t totalBytesReadSem;
size_t totalBytesRead;
int bytes;
int randomByteIndex;
char randomChar[RANDOM_CHAR_READ_BLOCK_SIZE];
unsigned char max;

void CleanFile(int fd) {
    char filename[5];
    sprintf(filename, "%d", fd);
    int file = open(filename, O_RDWR | O_CREAT | O_TRUNC, (mode_t) 0600);
    if (file == -1) {
        perror("Can't open");
        exit(EXIT_FAILURE);
    }
    close(file);
}

void CleanFiles(size_t filesAmount, int * files) {
    int i;
    char filename[5];
    for (i = 0; i < filesAmount; i++) {
        sprintf(filename, "%d", i);
        files[i] = open(filename, O_RDWR | O_CREAT | O_TRUNC, (mode_t) 0600);
        if (files[i] == -1) {
            perror("Can't open");
            exit(EXIT_FAILURE);
        }
        close(files[i]);
    }
}

void OpenFiles(size_t filesAmount, int * files) {
    int i;
    char filename[5];
    for (i = 0; i < filesAmount; i++) {
        sprintf(filename, "%d", i);
        files[i] = open(filename, O_RDWR | O_CREAT | O_APPEND, (mode_t) 0600);
        posix_fadvise(files[i], 0, 0, POSIX_FADV_DONTNEED);
        if (files[i] == -1) {
            perror("Can't open");
            exit(EXIT_FAILURE);
        }
    }
}

void OpenFile(int fd) {
    char filename[5];
    sprintf(filename, "%d", fd);
    int file_d = open(filename, O_RDWR | O_CREAT | O_APPEND, (mode_t) 0600);

    if (file_d == -1) {
        perror("Can't open");
        exit(EXIT_FAILURE);
    }
}

unsigned char ReadChar(int randomFD) {
    if (randomByteIndex < RANDOM_CHAR_READ_BLOCK_SIZE) {
        randomByteIndex += 1;
        return randomChar[randomByteIndex];
    } else {
        size_t result = read(randomFD, &randomChar, sizeof(randomChar));
        if (result == -1) {
            perror("Can't read int from /dev/urandom\n");
            return 0;
        }
        randomByteIndex = 0;
        return randomChar[randomByteIndex];
    }
}

void* ReadFile(void* args) {
    struct ReadFromFileArgs* fileArgs = (struct ReadFromFileArgs*) args;
    unsigned char readBlock[IO_BLOCK_SIZE];
    size_t readBytes;

    while (1) {
        readBytes = read(fileArgs->fd, &readBlock, IO_BLOCK_SIZE);
        if (readBytes == -1) {
            perror("Error reading from file");
            break;
        }
#ifdef LOG
        sem_wait(&totalBytesReadSem);
        totalBytesRead += readBytes;
        sem_post(&totalBytesReadSem);
#endif
        if (readBytes < IO_BLOCK_SIZE) {
            if (readBytes == 0) {
                break;
            }
        } else {
            for (int i = 0; i < IO_BLOCK_SIZE; i++) {
                if (readBlock[i] > max) {
                    max = readBlock[i];
                }
            }
        }
    }
    sleep(1);
}

void* WriteToMemory(void* args) {
    struct WriteToMemoryArgs* writeArgs = (struct WriteToMemoryArgs*) args;
    unsigned int bytesWrote = 0;
    for (int i = writeArgs->start; i < writeArgs->end; i += 1) {
        unsigned char random = ReadChar(writeArgs->randomFD);
        writeArgs->memoryRegion[i] = random;
        bytesWrote += 1;
    }

#ifdef LOG
    printf("Thread with ID %lu finished and wrote %u bytes\n", writeArgs->threadId, bytesWrote);
#endif
    free(writeArgs);
}

void WriteToFile(const unsigned char* memoryRegion, int fd, size_t fileNum, size_t bytesCount) {
    unsigned char ioBlock[IO_BLOCK_SIZE];
    int ioBlockByte = 0;
    int totalBytesWrittenToFile = 0;
    size_t bytesWritten;
    size_t i;
    for (i = fileNum * OUTPUT_FILE_SIZE; i < fileNum * OUTPUT_FILE_SIZE + bytesCount; i++) {
        ioBlock[ioBlockByte] = memoryRegion[i];
        ioBlockByte += 1;
        if (ioBlockByte >= IO_BLOCK_SIZE) {
            ioBlockByte = 0;
            bytesWritten = write(fd, &ioBlock, IO_BLOCK_SIZE);
            if (bytesWritten == -1) {
                perror("Can't write to file");
                return;
            }
            totalBytesWrittenToFile += bytesWritten;
        }
    }

    if (ioBlockByte > 0) {
        bytesWritten = write(fd, &ioBlock, IO_BLOCK_SIZE - ioBlockByte);
        if (bytesWritten == -1) {
            perror("Can't write to file");
            return;
        }
        totalBytesWrittenToFile += bytesWritten;
    }
#ifdef LOG
    printf("Total bytes written to file: %d\n", totalBytesWrittenToFile);
#endif
}

_Noreturn void* WriteToFiles(void* args) {
    printf("Started writing to files\n");
    struct WriteToFilesArgs* writeToFilesArgs = (struct WriteToFilesArgs*) args;
    int i = 0;
    while (1) {
        for (i = 0; i < writeToFilesArgs->filesAmount; i++) {
            CleanFile(i);
            OpenFile(i);
            if (writeToFilesArgs->fileSizeQuotient != 0 && i == writeToFilesArgs->filesAmount - 1) {
                WriteToFile(writeToFilesArgs->memoryRegion, writeToFilesArgs->files[i], i, writeToFilesArgs->fileSizeQuotient);
            } else {
                WriteToFile(writeToFilesArgs->memoryRegion, writeToFilesArgs->files[i], i, writeToFilesArgs->fileSizeRemainder);
            }
        }
    }
}

void* WriteToFilesOnce(void* args) {
    struct WriteToFilesArgs* writeToFilesArgs = (struct WriteToFilesArgs*) args;
    int i = 0;
    struct timespec start, finish;
    double elapsed;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (i = 0; i < writeToFilesArgs->filesAmount; i++) {
        CleanFile(i);
        OpenFile(i);
        if (writeToFilesArgs->fileSizeQuotient != 0 && i == writeToFilesArgs->filesAmount - 1) {
            WriteToFile(writeToFilesArgs->memoryRegion, writeToFilesArgs->files[i], i, writeToFilesArgs->fileSizeQuotient);
        } else {
            WriteToFile(writeToFilesArgs->memoryRegion, writeToFilesArgs->files[i], i, writeToFilesArgs->fileSizeRemainder);
        }
        close(writeToFilesArgs->files[i]);
    }
    clock_gettime(CLOCK_MONOTONIC, &finish);

    elapsed = (finish.tv_sec - start.tv_sec);
    elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
    printf("Writing to files took: %f seconds\n", elapsed);
}