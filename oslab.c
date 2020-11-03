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
  int fileNumber;
  size_t block;
  int fileSize;
  int tid;
  int upperBound;
} writeF_data;

pthread_mutex_t writeMutex = PTHREAD_MUTEX_INITIALIZER;
void *fillMalloc(void *arg);
void *writeFile(void *arg);

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
    int tid;
    int files[5];

    for (int f = 0; f < 5; f++) {
      char *name = (char *) malloc(sizeof(char));
      sprintf(name, "%i", f);
      int file = open(name, O_RDWR | O_APPEND | O_CREAT | O_DSYNC, 0644);
      free(name);
      files[f] = file;
    }

    for (int j = 0; j < Ebytes / G; j++) {
      for (int i = 0; i < 5; i++) {
        writeF_data[i].start = start;
        writeF_data[i].fileNumber = i;
        writeF_data[i].block = (size_t) G;
        writeF_data[i].fileSize = 1050;
        writeF_data[i].tid = pthread_create(&writeF[i], NULL, writeFile, &writeF_data[i]);
        writeF_data[i].upperBound = numOfInts;
        pthread_join(writeF[i], NULL);
      }
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
  char *name = (char *) malloc(sizeof(char));
  sprintf(name, "%i", data->fileNumber);
  int file = open(name, O_WRONLY | O_APPEND | O_CREAT | O_DSYNC, 0644);
  free(name);

  int random, result;
  int* buf = NULL;
  int loop = (int)( (data->fileSize) / (data->block) );

  for (int i = 0; i < loop; i++) {
    buf = data->start + rand() % data->upperBound - data->block;
    result = write(file, buf, data->block);
    if (result == -1) {
      printf("Thread no.%i Write failed: %s\n", data->tid, strerror(errno));
      close(file);
      pthread_mutex_unlock(&writeMutex);
      pthread_exit(NULL);
    }
  }
  printf("Wrote data to file no.%i\n", data->fileNumber);
  close(file);
  pthread_mutex_unlock(&writeMutex);
  pthread_exit(NULL);
}
