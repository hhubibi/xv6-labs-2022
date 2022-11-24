#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
filter(int p[2])
{
  int n;
  int base;
  int p2[2];

  close(p[1]);
  if(read(p[0], &base, sizeof(int)) == 0){
    close(p[0]);
    return;
  }
  fprintf(1, "prime %d\n", base);

  if(pipe(p2) < 0){
    fprintf(2, "pipe() failed\n");
    exit(1);
  }

  if(fork() == 0){
    filter(p2);
  } else {
    close(p2[0]);
    while(read(p[0], &n, sizeof(int)) > 0){
      if(n % base != 0){
        write(p2[1], &n, sizeof(int));
      }
    }
    close(p[0]);
    close(p2[1]);
    wait(0);
  }
  exit(0);
}

int
main(int argc, char *argv[])
{
  int i;
  int p[2];

  if(argc != 1){
    fprintf(2, "Usage: primes\n");
    exit(1);
  }

  if(pipe(p) < 0){
    fprintf(2, "pipe() failed\n");
    exit(1);
  }

  if(fork() == 0){
    filter(p);
  } else {
    close(p[0]);
    fprintf(1, "prime 2\n");
    for (i = 3; i <= 35; i += 2){
      write(p[1], &i, sizeof(int));
    }
    close(p[1]);
    wait(0);
  }
  
  exit(0);
}