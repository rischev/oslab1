#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>

typedef struct _malloc_data {
  int *address;
  FILE* input;
  int chunk;
} malloc_data;

typedef struct _writeF_data {
  char *start;
  size_t block;
  int file;
  int tid;
  int upperBound;
} writeF_data;

typedef struct _readF_data {
  int file;
  int tid;
  int *acc;
  int *ct;
  off_t offset;
} readF_data;

pthread_mutex_t countMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mallocMutex = PTHREAD_MUTEX_INITIALIZER;
void *fillMalloc(void* arg);
void *writeFile(void* arg);
void *readFile(void* arg);

int main() {
  const int A = 266;
  const int Abytes = A * 1024 * 1024;
  const char C[] = "malloc";
  const int D = 79;
  const int E = 100;
  const int Ebytes = E * 1024 * 1024;
  const char F[] = "block";
  const int G = 105;
  const char H[] = "random";
  const int I = 39;
  const char J[] = "avg";
  const char K[] = "flock";
  const char numOfFiles = 6;

  pthread_t mallocThr[D];
  malloc_data mallocThr_data[D];
  int* start = (int*) malloc(Abytes), *nxt = start;
  int numOfInts = Abytes / sizeof(int), length = numOfInts / D;
  FILE* rFile = fopen("/dev/urandom", "r");

  if (start != NULL) {
    for ( ; ; ) {
      for (int i = 0; i < D; i++) {
        mallocThr_data[i].input = rFile;
        mallocThr_data[i].address = nxt;
        nxt += length;
        if (i == D - 1) { mallocThr_data[i].chunk = length + numOfInts % D; }
        else { mallocThr_data[i].chunk = length; }
        pthread_create(&mallocThr[i], NULL, fillMalloc, &mallocThr_data[i]);
      }

      for (int i = 0; i < D; i++) { pthread_join(mallocThr[i], NULL); }

      pthread_t writeF[I], readF[I];
      writeF_data writeF_data[I];
      readF_data readF_data[I];
      int tid, fileId, files[numOfFiles];
      int allSum = 0, allCt = 0;

      for (int f = 0; f < numOfFiles; f++) {
        char *name = (char*) malloc(sizeof(char));
        sprintf(name, "%i", f);
        files[f] = open(name, O_RDWR | O_APPEND | O_CREAT | O_DSYNC, 0644);
        free(name);
      }

      for (int i = 0; i < I; i++) {
        fileId = i % numOfFiles;
        writeF_data[i].file = files[fileId];
        writeF_data[i].start = (char*) start;
        writeF_data[i].block = (size_t) G;
        writeF_data[i].upperBound = numOfInts;

        readF_data[fileId].file = files[fileId];
        readF_data[fileId].acc = &allSum;
        readF_data[fileId].ct = &allCt;
        readF_data[fileId].offset = (off_t) G;
      }

      for (int j = 1; j < Ebytes / G; j++) {
        for (int i = 0; i < I; i++) { writeF_data[i].tid = pthread_create(&writeF[i], NULL, writeFile, &writeF_data[i]); }
        for (int k = 0; k < numOfFiles; k++) { readF_data[k].tid = pthread_create(&readF[k], NULL, readFile, &readF_data[k]); }
        for (int a = 0; a < I; a++) { pthread_join(writeF[a], NULL); }
        for (int b = 0; b < numOfFiles; b++) { pthread_join(readF[b], NULL); }
        if (j % 300 == 0) { printf("Current acc = %i, ct = %i, avg = %f\n", allSum, allCt, (double)allSum / (double)allCt); }
      }

      for (int f = 0; f < numOfFiles; f++) { close(files[f]); }
    }
  }
  else {
    printf("Could not allocate memory");
  }
  fclose(rFile);
  free(start);
  return EXIT_SUCCESS;
}

void *fillMalloc(void* arg) {
  pthread_mutex_lock(&mallocMutex);
  malloc_data *data = (malloc_data *) arg;
  for (int i = 0; i < data->chunk; i++) {
    *(data->address) = getw(data->input);
    (data->address)++;
  }
  pthread_mutex_unlock(&mallocMutex);
  pthread_exit(NULL);
}

void *writeFile(void* arg) {
  writeF_data *data = (writeF_data *) arg;
  int result;
  char* buf = (char*) malloc(data->block);
  if (buf == NULL) {
    printf("Could not allocate write buffer\n");
    pthread_exit(NULL);
  }
  char* random = data->start + rand() % data->upperBound - data->block;
  for (int i = 0; i < data->block; i++) { buf[i] = *(random++); }

  flock(data->file, LOCK_EX);
  lseek(data->file, 0L, SEEK_END);
  result = write(data->file, buf, data->block);
  free(buf);
  flock(data->file, LOCK_UN);

  if (result == -1) { printf("Write thread no.%i failed: %s\n", data->tid, strerror(errno)); }
  pthread_exit(NULL);
}

void *readFile(void* arg) {
  readF_data *data = (readF_data *) arg;
  char* buf = (char *) malloc(data->offset);
  if (buf == NULL) {
    printf("Could not allocate read buffer\n");
    pthread_exit(NULL);
  }

  flock(data->file, LOCK_EX);
  lseek(data->file, -(data->offset), SEEK_END);
  int result = read(data->file, buf, data->offset);
  flock(data->file, LOCK_UN);


  if (result == -1) {
    printf("Read thread no.%i failed: %s\n", data->tid, strerror(errno));
    free(buf);
    pthread_exit(NULL);
  }

  pthread_mutex_lock(&countMutex);
  for (int i = 0; i < data->offset; i++) { *(data->acc) += (int) buf[i]; }
  *(data->ct) += data->offset;
  free(buf);
  pthread_mutex_unlock(&countMutex);

  pthread_exit(NULL);
}
