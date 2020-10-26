#include <stdio.h>
#include <sys/mman.h>
#include <math.h>
#include <fcntl.h>
#include <zconf.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>


#include "IO.h"
#include "Constants.h"

// A=276;B=0x28B070E0;C=mmap;D=74;E=47;F=nocache;G=36;H=random;I=139;J=max;K=sem

extern sem_t totalBytesReadSem;
extern size_t totalBytesRead;
extern int bytes;

unsigned char max;

int main() {
    sem_t sem;
    sem_t totalBytesReadSem;
    int bytes;
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

    printf("After input char program will allocate memory\n");
    getchar();

    unsigned char* memoryRegion = mmap(
            (void*) ADDRESS,
            bytes,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            outputFD, 0);

    if (memoryRegion == MAP_FAILED) {
        close(outputFD);
        perror("Error mapping a file");
        exit(EXIT_FAILURE);
    }
    char ch;
    while ((ch = getchar()) != '\n' && ch != EOF);
    printf("After input char program will continue writing data to memory\n");
    getchar();

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

    struct timespec start;

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

    struct timespec finish;
    double elapsed;

    clock_gettime(CLOCK_MONOTONIC, &finish);

    elapsed = (finish.tv_sec - start.tv_sec);
    elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;

    printf("Filling up the memory took: %f seconds\n", elapsed);

    while ((ch = getchar()) != '\n' && ch != EOF);
    printf("After input char program will continue writing data to file\n");
    getchar();
//-------------------------------------WRITES RANDOM DATA TO FILES-------------------------------------

    int filesAmount = (ALLOCATE_MBYTES / OUTPUT_FILE_SIZE) + 1;
//#ifdef LOG
    printf("Files amount: %d\n", filesAmount);
//#endif
    int files[filesAmount];
    sem_t * fileSems = malloc(sizeof(sem_t) * filesAmount);
    CleanFiles(filesAmount, files);
    OpenFiles(filesAmount, files);
    printf("Here\n");
    pthread_t writeToFilesThreadId;

    for (i = 0; i < filesAmount; i++) {
        sem_init(&fileSems[i], 0, 1);
    }

    struct WriteToFilesArgs* writeToFilesArgs = malloc(sizeof(struct WriteToFilesArgs));

//    clock_gettime(CLOCK_MONOTONIC, &start);

    int fileSizeRemainder = bytes / (filesAmount);
    int fileSizeQuotient = bytes % (filesAmount);

    writeToFilesArgs->fileSizeQuotient = fileSizeQuotient;
    writeToFilesArgs->fileSizeRemainder = fileSizeRemainder;
    writeToFilesArgs->files = files;
    writeToFilesArgs->fileSems = fileSems;
    writeToFilesArgs->filesAmount = filesAmount;
    writeToFilesArgs->memoryRegion = memoryRegion;

    WriteToFilesOnce(writeToFilesArgs);

//    if (pthread_create(&writeToFilesThreadId, NULL, WriteToFiles, writeToFilesArgs)) {
//        free(writeToFilesArgs);
//        perror("Can't create write to files thread");
//    }

//    clock_gettime(CLOCK_MONOTONIC, &finish);
//
//    elapsed = (finish.tv_sec - start.tv_sec);
//    elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
//
//    printf("Writing to files took: %f seconds\n", elapsed);

//-------------------------------------READS AND AGGREGATES RANDOM DATA FROM FILES-------------------------------------

    pthread_t readFromFileThreads[READ_THREADS_AMOUNT];

    int fileIndex = 0;

    OpenFiles(filesAmount, files);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (i = 0; i < READ_THREADS_AMOUNT; i++) {
        struct ReadFromFileArgs* fileArgs = malloc(sizeof(struct ReadFromFileArgs));
        if (fileIndex >= filesAmount) fileIndex = 0;
        fileArgs->fd = files[fileIndex];
        fileArgs->sem = fileSems[fileIndex];
        fileArgs->fileIndex = fileIndex;
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
        close(files[i]);
    }

    pthread_cancel(writeToFilesThreadId);

    if (munmap(memoryRegion, bytes) == -1) {
//        close(outputFD);
        close(randomFD);
        perror("Error un-mmapping the file");
        exit(EXIT_FAILURE);
    }

    while ((ch = getchar()) != '\n' && ch != EOF);
    printf("After input char program will terminate\n");
    getchar();

    if (close(outputFD)) {
        printf("Error in closing file.\n");
    }

    if (close(randomFD)) {
        printf("Error in closing file.\n");
    }

    return 0;
}