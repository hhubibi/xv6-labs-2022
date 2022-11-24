#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int
main(int argc, char *argv[])
{
  char *cmd_argv[MAXARG];
  char buf[512], c;
  int n, i, j;

  if(argc < 2){
    fprintf(2, "Usage: xargs cmd ...\n");
    exit(1);
  }

  for(i = 1; i < argc; i++){
    cmd_argv[i-1] = argv[i];
  }

  j = 0;
  while((n = read(0, &c, 1)) > 0){
    if (c == '\n'){
      buf[j] = '\0';
      cmd_argv[argc-1] = buf;
      j = 0;
      if(fork() == 0){
        exec(argv[1], cmd_argv);
      }
    } else {
      buf[j++] = c;
    }
  }

  exit(0);
}