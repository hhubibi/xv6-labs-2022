#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int p[2];
  char buf[1];

  if(argc != 1){
    fprintf(2, "Usage: pingpong\n");
    exit(1);
  }

  if(pipe(p) < 0){
    fprintf(2, "pipe() failed\n");
    exit(1);
  }

  if(fork() == 0){
    read(p[0], buf, 1);
    close(p[0]);
    fprintf(1, "%d: received ping\n", getpid());
    write(p[1], buf, 1);
    close(p[1]);
    exit(0);
  } else {
    write(p[1], " ", 1);
    close(p[1]);
    read(p[0], buf, 1);
    close(p[0]);
    wait(0);
    fprintf(1, "%d: received pong\n", getpid());
  }

  exit(0);
}