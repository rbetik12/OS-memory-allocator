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
#define THREADS_AMOUNT 8
#define ALLOCATE_BYTES 10

//#define LOG_MEMORY_PROGRESS 0

// A=276;B=0x28B070E0;C=mmap;D=74;E=47;F=nocache;G=36;H=random;I=139;J=max;K=sem

sem_t sem;
unsigned long totalBytesWrote = 0;
int mBytes = 0;
int randomByteIndex = RANDOM_CHAR_READ_BLOCK_SIZE;
char randomChar[RANDOM_CHAR_READ_BLOCK_SIZE];

struct WriteToMemoryArgs {
    int randomFD;
    unsigned char* memoryRegion;
    int mBytes;
    int start;
    int end;
    pthread_t threadId;
};

unsigned char ReadChar(int randomFD) {
    if (randomByteIndex < RANDOM_CHAR_READ_BLOCK_SIZE) {
        randomByteIndex += 1;
        return randomChar[randomByteIndex];
    }
    else {
        size_t result = read(randomFD, &randomChar, sizeof(randomChar));
        if (result == -1) {
            perror("Can't read int from /dev/urandom\n");
            return 0;
        }
        randomByteIndex = 0;
        return randomChar[randomByteIndex];
    }
}

void* WriteToMemory(void* args) {
    struct WriteToMemoryArgs* writeArgs = (struct WriteToMemoryArgs*) args;
    unsigned int bytesWrote = 0;
    for (int i = writeArgs->start; i < writeArgs->end; i += 1) {
        unsigned char random = ReadChar(writeArgs->randomFD);
        writeArgs->memoryRegion[i] = random;
        bytesWrote += 1;
#ifdef LOG_MEMORY_PROGRESS
        sem_wait(&sem);
        totalBytesWrote += 1;
        if (totalBytesWrote % 100 == 0)
            printf("%lu/%d\n", totalBytesWrote, mBytes);
        sem_post(&sem);
#endif
    }

    printf("Thread with ID %lu finished and wrote %u bytes\n", writeArgs->threadId, bytesWrote);
    free(writeArgs);
}

int main() {
    sem_init(&sem, 0, 1);
    mBytes = ALLOCATE_BYTES * pow(10, 6);

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

    size_t fileSize = mBytes + 1;

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

    unsigned char* memoryRegion = NULL;

    memoryRegion = mmap(
            (void*) memoryRegion,
            mBytes,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            outputFD, 0);

    if (memoryRegion == MAP_FAILED) {
        close(outputFD);
        perror("Error mapping a file");
        exit(EXIT_FAILURE);
    }

    printf("Size of pointer: %d\n", sizeof(*memoryRegion));

    const int memoryRemainder = mBytes / THREADS_AMOUNT / sizeof(*memoryRegion);
    const int memoryQuotient = mBytes % THREADS_AMOUNT / sizeof(*memoryRegion);

    printf("Remainder: %d\n", memoryRemainder);
    printf("Quotient: %d\n", memoryQuotient);

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
        args->mBytes = mBytes;

        args->start = i * memoryRemainder;
        args->end = ++i * memoryRemainder;


        if (pthread_create(&writeToMemoryThreads[thread], NULL, WriteToMemory, (void*) args)) {
            free(args);
            perror("Can't create thread");
        }
        args->threadId = writeToMemoryThreads[thread];
//        WriteToMemory(args);
        thread += 1;
    }


    if (memoryQuotient != 0) {
        args = malloc(sizeof(*args));
        args->memoryRegion = memoryRegion;
        args->randomFD = randomFD;
        args->mBytes = mBytes;

        args->start = i * memoryRemainder;
        args->end = i * memoryRemainder + memoryQuotient;
        WriteToMemory((void *) args);
    }

    for (int i = 0; i < THREADS_AMOUNT; i++) {
        pthread_join(writeToMemoryThreads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &finish);

    elapsed = (finish.tv_sec - start.tv_sec);
    elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;

    printf("Filling up the memory took: %f seconds\n", elapsed);

    if (msync(memoryRegion, mBytes, MS_SYNC) == -1) {
        perror("Could not sync the file to disk");
    }

    if (munmap(memoryRegion, mBytes) == -1) {
        close(outputFD);
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