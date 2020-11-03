#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

typedef struct _malloc_data {
  int *address;
  FILE* input;
  int chunk;
} malloc_data;

typedef struct _writeF_data {
  int *start;
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

pthread_mutex_t writeMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t readMutex = PTHREAD_MUTEX_INITIALIZER;
void *fillMalloc(void *arg);
void *writeFile(void *arg);
void *readFile(void * arg);

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
  const char numOfFiles = 5;

  pthread_t mallocThr[D];
  malloc_data mallocThr_data[D];
  int* start = (int*) malloc(Abytes);
  int* nxt = start;
  int numOfInts = Abytes / sizeof(int);
  int length = numOfInts / D;
  FILE* rFile = fopen("/dev/urandom", "r");

  if (start != NULL) {
    //while (true) {
    for (int i = 0; i < D; i++) {
      mallocThr_data[i].input = rFile;
      mallocThr_data[i].address = nxt;
      nxt += length;
      if (i == D - 1) {
        mallocThr_data[i].chunk = length + numOfInts % D;
      }
      else {
        mallocThr_data[i].chunk = length;
      }
      pthread_create(&mallocThr[i], NULL, fillMalloc, &mallocThr_data[i]);
      pthread_join(mallocThr[i], NULL);
    }

    pthread_t writeF[numOfFiles];
    writeF_data writeF_data[numOfFiles];
    pthread_t readF[numOfFiles];
    readF_data readF_data[numOfFiles];
    int tid;
    int files[numOfFiles];
    int allSum = 0, allCt = 1;

    for (int f = 0; f < numOfFiles; f++) {
      char *name = (char *) malloc(sizeof(char));
      sprintf(name, "%i", f);
      int file = open(name, O_RDWR | O_APPEND | O_CREAT | O_DSYNC, 0644);
      free(name);
      files[f] = file;

      writeF_data[f].file = files[f];
      writeF_data[f].start = start;
      writeF_data[f].block = (size_t) G;
      writeF_data[f].upperBound = numOfInts;

      readF_data[f].file = files[f];
      readF_data[f].acc = &allSum;
      readF_data[f].ct = &allCt;
      readF_data[f].offset = (off_t) G;
    }

    for (int j = 0; j < Ebytes / G; j++) {
      for (int i = 0; i < numOfFiles; i++) {
        writeF_data[i].tid = pthread_create(&writeF[i], NULL, writeFile, &writeF_data[i]);
        pthread_join(writeF[i], NULL);
        readF_data[i].tid = pthread_create(&readF[i], NULL, readFile, &readF_data[i]);
        pthread_join(readF[i], NULL);
      }
      if (j % 1000 == 0) {
        printf("Current acc = %i, ct = %i\n", allSum, allCt);
      }
    }

    for (int f = 0; f < numOfFiles; f++) {
      close(files[f]);
    }


    //}
  }
  else {
    printf("Couldn't allocate memory");
  }

  free(start);
  return EXIT_SUCCESS;
}

void *fillMalloc(void *arg) {
  malloc_data *data = (malloc_data *) arg;
  int randomData;
  for (int i = 0; i < data->chunk; i++) {
    randomData = getw(data->input);
    *(data->address) = randomData;
    (data->address)++;
  }
  pthread_exit(NULL);
}

void *writeFile(void *arg) {
  pthread_mutex_lock(&writeMutex);
  writeF_data *data = (writeF_data *) arg;
  int random, result;
  int* buf = NULL;
  buf = data->start + rand() % data->upperBound - data->block;
  result = write(data->file, buf, data->block);
  if (result == -1) {
    printf("Write thread no.%i failed: %s\n", data->tid, strerror(errno));
    pthread_mutex_unlock(&writeMutex);
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(&writeMutex);
  pthread_exit(NULL);
}

void *readFile(void *arg) {
  pthread_mutex_lock(&readMutex);
  readF_data *data = (readF_data *) arg;
  char* buf = (char *) malloc(data->offset);
  lseek(data->file, -(data->offset), SEEK_END);
  int result = read(data->file, buf, data->offset);
  if (result == -1) {
    printf("Read thread no.%i failed: %s\n", data->tid, strerror(errno));
    pthread_mutex_unlock(&readMutex);
    pthread_exit(NULL);
    free(buf);
  }
  for (int i = 0; i < data->offset; i++) {
    *(data->acc) += (int)buf[i];
    *(data->ct) += 1;
  }
  free(buf);
  pthread_mutex_unlock(&readMutex);
  pthread_exit(NULL);
}
