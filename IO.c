#include "IO.h"
#include "Constants.h"

//sem_t totalBytesReadSem;
size_t totalBytesRead;
int bytes;
int randomByteIndex;
char randomChar[RANDOM_CHAR_READ_BLOCK_SIZE];
unsigned char max;

void CleanFile(int fd) {
    char filename[5];
    sprintf(filename, "%d", fd);
    FILE* file = fopen(filename, "wb+");
    if (file == NULL) {
        perror("Can't open");
        exit(EXIT_FAILURE);
    }
    fclose(file);
}

void CleanFiles(size_t filesAmount, FILE** files) {
    int i;
    char filename[5];
    for (i = 0; i < filesAmount; i++) {
        sprintf(filename, "%d", i);
        files[i] = fopen(filename, "wb+");
        if (files[i] == NULL) {
            perror("Can't open");
            exit(EXIT_FAILURE);
        }
        fclose(files[i]);
    }
}

void OpenFiles(size_t filesAmount, FILE** files) {
    int i;
    char filename[5];
    for (i = 0; i < filesAmount; i++) {
        sprintf(filename, "%d", i);
        files[i] = fopen(filename, "ab+");
        if (files[i] == NULL) {
            perror("Can't open");
            exit(EXIT_FAILURE);
        }
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
        readBytes = fread(readBlock, sizeof(unsigned char), IO_BLOCK_SIZE, fileArgs->file);
#ifdef LOG
        sem_wait(&totalBytesReadSem);
        totalBytesRead += readBytes;
        sem_post(&totalBytesReadSem);
#endif
        if (readBytes < IO_BLOCK_SIZE) {
            if (feof(fileArgs->file)) {
                break;
            } else {
                perror("Error reading file\n");
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
    free(fileArgs);
}

void* WriteToMemory(void* args) {
    struct WriteToMemoryArgs* writeArgs = (struct WriteToMemoryArgs*) args;
    unsigned int bytesWrote = 0;
    for (int i = writeArgs->start; i < writeArgs->end; i += 1) {
        unsigned char random = ReadChar(writeArgs->randomFD);
        writeArgs->memoryRegion[i] = random;
        bytesWrote += 1;
//#ifdef LOG
//        sem_wait(&sem);
//        totalBytesWrote += 1;
//        if (totalBytesWrote % 1000 == 0)
//            printf("%lu/%d\n", totalBytesWrote, bytes);
//        sem_post(&sem);
//#endif
    }

#ifdef LOG
    printf("Thread with ID %lu finished and wrote %u bytes\n", writeArgs->threadId, bytesWrote);
#endif
    free(writeArgs);
}

void WriteToFile(const unsigned char* memoryRegion, FILE* file, size_t fileNum, size_t bytesCount) {
    unsigned char ioBlock[IO_BLOCK_SIZE];
    int ioBlockByte = 0;
    int totalBytesWrittenToFile = 0;
    for (size_t i = fileNum * OUTPUT_FILE_SIZE; i < fileNum * OUTPUT_FILE_SIZE + bytesCount; i++) {
        ioBlock[ioBlockByte] = memoryRegion[i];
        ioBlockByte += 1;
        if (ioBlockByte >= IO_BLOCK_SIZE) {
            fwrite(&ioBlock, sizeof(unsigned char), IO_BLOCK_SIZE, file);
            ioBlockByte = 0;
            totalBytesWrittenToFile += IO_BLOCK_SIZE;
        }
    }

    if (ioBlockByte > 0) {
        fprintf(file, "%s", ioBlock);
        totalBytesWrittenToFile += ioBlockByte + 1;
    }

    fflush(file);
#ifdef LOG
    printf("Total bytes written to file: %d\n", totalBytesWrittenToFile);
#endif
}