#include <ctype.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "outputs.h"


OutputInfo
*outputs_set(int *count_out)
{
  OutputInfo *outputs = NULL;
  int noutputs = 0;

  int pipefd[2];
  if (pipe(pipefd) == -1) {
    perror("pipe");
    return NULL;
  }

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    return NULL;
  }

  if (pid == 0) {
    // child writes to pipe
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);

    // connector width height x-offset y-offset
    system("xrandr | sed -n 's/^\\(.*\\)\\sconnected[^0-9-]*\\([0-9]\\{1,\\}\\)x\\([0-9]\\{1,\\}\\)+\\([0-9]\\{1,\\}\\)+\\([0-9]\\{1,\\}\\).*$/\\1 \\2 \\3 \\4 \\5/p'");

    close(pipefd[1]);
    _exit(0);
  } else {
    // parent parses the numbers
    close(pipefd[1]);

    int state = 0;
    int num = 0;

    char buf;
    char *name;
    int name_len;
    while (read(pipefd[0], &buf, 1) > 0) {
      switch (state) {
        case 0: // start of input, after newline
          if (isspace(buf) != 0)
            break;

          state = 1;
          outputs = realloc(outputs, ++noutputs * sizeof(OutputInfo));

          outputs[noutputs - 1].idx = noutputs - 1;
          name = malloc(sizeof(char) * 256);
          name_len = 0;
          // pass:

        case 1: // read name
          if (isspace(buf) != 0) {
            name[name_len] = '\0';
            outputs[noutputs - 1].name = name;
            state = 2;
            continue;
          }

          name[name_len] = buf;
          name_len++;
          continue;

#define STATE(CASE, FIELD, NEXT_CASE) \
        case CASE:\
          if (isdigit(buf) == 0) {\
            outputs[noutputs - 1].FIELD = num;\
            \
            state = NEXT_CASE;\
            num = 0;\
            continue;\
          }\
          \
          num *= 10;\
          num += buf - '0';\
          \
          break;

        STATE(2, w, 3); // reading width
        STATE(3, h, 4); // reading height
        STATE(4, x, 5); // reading left
        STATE(5, y, 0); // reading top
#undef STATE
      }
    }

    wait(NULL);
    close(pipefd[0]);
  }

  *count_out = noutputs;
  return outputs;
}

void
outputs_print(OutputInfo o)
{
  printf("output idx: %d, name: %s, size: %dx%d, pos: +%d +%d\n", o.idx, o.name, o.w, o.h, o.x, o.y);
}

void
outputs_free(OutputInfo *outputs)
{
  if (outputs) {
    int noutputs = sizeof(&outputs) / sizeof(OutputInfo);
    for (int i = 0; i < noutputs; i++) {
      free(outputs[i].name);
    }
    free(outputs);
  }
}
