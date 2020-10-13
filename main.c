#include <stdio.h>
#include <sys/mman.h>
#include <math.h>
#include <fcntl.h>
#include <zconf.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>

#define RANDOM_CHAR_READ_BLOCK_SIZE 10000
#define THREADS_AMOUNT 74
#define ALLOCATE_MBYTES 276
#define OUTPUT_FILE_SIZE 47
#define IO_BLOCK_SIZE 36
#define READ_THREADS_AMOUNT 139
//#define LOG

// A=276;B=0x28B070E0;C=mmap;D=74;E=47;F=nocache;G=36;H=random;I=139;J=max;K=sem

sem_t sem;
sem_t totalBytesReadSem;
size_t totalBytesWrote = 0;
size_t totalBytesRead = 0;
int bytes = 0;
int randomByteIndex = RANDOM_CHAR_READ_BLOCK_SIZE;
char randomChar[RANDOM_CHAR_READ_BLOCK_SIZE];
unsigned char max = 0;

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

int main() {
    sem_init(&totalBytesReadSem, 0, 1);
    sem_init(&sem, 0, 1);
    bytes = ALLOCATE_MBYTES * pow(10, 6);

    int outputFD = open("output", O_RDWR | O_CREAT | O_TRUNC, (mode_t) 0600);
    int randomFD = open("/dev/urandom", O_RDONLY);

    if (randomFD == -1) {
        perror("Can't read /dev/urandom\n");
        exit(EXIT_FAILURE);
    }

    if (outputFD == -1) {
        perror("Can't open file\n");
        exit(EXIT_FAILURE);
    }

    size_t fileSize = bytes + 1;

    if (lseek(outputFD, fileSize - 1, SEEK_SET) == -1) {
        close(outputFD);
        perror("Error calling lseek() to 'stretch' the file");
        exit(EXIT_FAILURE);
    }

    if (write(outputFD, "", 1) == -1) {
        close(outputFD);
        perror("Error writing last byte of the file");
        exit(EXIT_FAILURE);
    }

    unsigned char* memoryRegion = (unsigned char*) 0x28B070E0;

    memoryRegion = mmap(
            (void*) memoryRegion,
            bytes,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            outputFD, 0);

    if (memoryRegion == MAP_FAILED) {
        close(outputFD);
        perror("Error mapping a file");
        exit(EXIT_FAILURE);
    }

//-------------------------------------WRITES RANDOM DATA TO MEMORY-------------------------------------

    const int memoryRemainder = bytes / THREADS_AMOUNT / sizeof(*memoryRegion);
    const int memoryQuotient = bytes % THREADS_AMOUNT / sizeof(*memoryRegion);
#ifdef LOG
    printf("Memory remainder: %d\n", memoryRemainder);
    printf("Memory quotient: %d\n", memoryQuotient);
#endif
    pthread_t writeToMemoryThreads[THREADS_AMOUNT];
    struct WriteToMemoryArgs* args;
    int thread = 0;
    int i;

    struct timespec start, finish;
    double elapsed;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (i = 0; i < THREADS_AMOUNT;) {
        args = malloc(sizeof(*args));
        args->memoryRegion = memoryRegion;
        args->randomFD = randomFD;
        args->mBytes = bytes;

        args->start = i * memoryRemainder;
        args->end = ++i * memoryRemainder;
        args->threadId = writeToMemoryThreads[thread];

        if (pthread_create(&writeToMemoryThreads[thread], NULL, WriteToMemory, (void*) args)) {
            free(args);
            perror("Can't create thread");
        }

        thread += 1;
    }


    if (memoryQuotient != 0) {
        args = malloc(sizeof(*args));
        args->memoryRegion = memoryRegion;
        args->randomFD = randomFD;
        args->mBytes = bytes;

        args->start = i * memoryRemainder;
        args->end = i * memoryRemainder + memoryQuotient;
        WriteToMemory((void*) args);
    }

    for (i = 0; i < THREADS_AMOUNT; i++) {
        pthread_join(writeToMemoryThreads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &finish);

    elapsed = (finish.tv_sec - start.tv_sec);
    elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;

    printf("Filling up the memory took: %f seconds\n", elapsed);

//-------------------------------------WRITES RANDOM DATA TO FILES-------------------------------------

    int filesAmount = (ALLOCATE_MBYTES / OUTPUT_FILE_SIZE) + 1;
#ifdef LOG
    printf("Files amount: %d\n", filesAmount);
#endif
    FILE* files[filesAmount];

    CleanFiles(filesAmount, files);
    OpenFiles(filesAmount, files);

    clock_gettime(CLOCK_MONOTONIC, &start);

    int fileSizeRemainder = bytes / (filesAmount);
    int fileSizeQuotient = bytes % (filesAmount);
    for (i = 0; i < filesAmount; i++) {
        if (fileSizeQuotient != 0 && i == filesAmount - 1) {
            WriteToFile(memoryRegion, files[i], i, fileSizeQuotient);
        } else {
            WriteToFile(memoryRegion, files[i], i, fileSizeRemainder);
        }
        fclose(files[i]);
    }

    clock_gettime(CLOCK_MONOTONIC, &finish);

    elapsed = (finish.tv_sec - start.tv_sec);
    elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;

    printf("Writing to files took: %f seconds\n", elapsed);

//-------------------------------------READS AND AGGREGATES RANDOM DATA FROM FILES-------------------------------------

    sem_t fileSems[filesAmount];
    pthread_t readFromFileThreads[READ_THREADS_AMOUNT];

    for (i = 0; i < filesAmount; i++) {
        sem_init(&fileSems[i], 0, 1);
    }
    int fileIndex = 0;

    OpenFiles(filesAmount, files);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (i = 0; i < READ_THREADS_AMOUNT; i++) {
        struct ReadFromFileArgs* fileArgs = malloc(sizeof(struct ReadFromFileArgs));
        if (fileIndex >= filesAmount) fileIndex = 0;
        fileArgs->file = files[fileIndex];
        fileArgs->sem = fileSems[fileIndex];
        fileIndex += 1;
        if (pthread_create(&readFromFileThreads[i], NULL, ReadFile, (void*) fileArgs)) {
            free(fileArgs);
            perror("Can't create thread");
        }
    }

    for (i = 0; i < READ_THREADS_AMOUNT; i++) {
        pthread_join(readFromFileThreads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &finish);

    elapsed = (finish.tv_sec - start.tv_sec);
    elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;

    printf("Reading from files: %f seconds\n", elapsed);

#ifdef LOG
    printf("Total bytes read: %lu\n", totalBytesRead);
#endif
    printf("Max value is: %d\n", max);

    for (i = 0; i < filesAmount; i++) {
        fclose(files[i]);
    }

    if (munmap(memoryRegion, bytes) == -1) {
//        close(outputFD);
        close(randomFD);
        perror("Error un-mmapping the file");
        exit(EXIT_FAILURE);
    }

    if (close(outputFD)) {
        printf("Error in closing file.\n");
    }

    if (close(randomFD)) {
        printf("Error in closing file.\n");
    }

    return 0;
}