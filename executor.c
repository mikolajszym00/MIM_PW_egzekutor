#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"
#include "err.h"

const int MAX_N_TASKS = 4096;
int free_task_id = 0;

struct Task {
    int read_dsc_out;
    int read_dsc_err;
};

void run(struct Task* tk, char** parts) {
    int pipe_dsc_out[2];
    ASSERT_SYS_OK(pipe(pipe_dsc_out));

    int pipe_dsc_err[2];
    ASSERT_SYS_OK(pipe(pipe_dsc_err));

    pid_t pid;
    ASSERT_SYS_OK(pid = fork());

    if (!pid) {
        ASSERT_SYS_OK(close(pipe_dsc_out[0])); // Close the read descriptor.
        ASSERT_SYS_OK(close(pipe_dsc_err[0])); // Close the read descriptor.

        printf("Task %d started: pid %d.\n", 0, getpid());

        ASSERT_SYS_OK(dup2(pipe_dsc_out[1], STDOUT_FILENO));
        ASSERT_SYS_OK(dup2(pipe_dsc_err[1], STDERR_FILENO));

        ASSERT_SYS_OK(close(pipe_dsc_out[1]));  // Close the original copy.
        ASSERT_SYS_OK(close(pipe_dsc_err[1]));  // Close the original copy.

        ASSERT_SYS_OK(execvp(parts[1], parts+1));
    } else {
        ASSERT_SYS_OK(close(pipe_dsc_out[1])); // Close the write descriptor.
        ASSERT_SYS_OK(close(pipe_dsc_err[1])); // Close the write descriptor.

        tk->read_dsc_out = pipe_dsc_out[0];
        tk->read_dsc_err = pipe_dsc_err[0];
    }
}

void out(char** parts) {


}


int main() {
    struct Task tasks[MAX_N_TASKS];

    char *buffer;
    size_t bufsize = 512;

//    while (true) {
        buffer = (char *)malloc(bufsize * sizeof(char));
        read_line(buffer, bufsize, stdin);

        printf("Type something: ");
        printf("You typed: '%s'\n", buffer);

        buffer[strcspn(buffer, "\n")] = 0;
        char** parts = split_string(buffer);

        int i = 0;
        while (parts[i] != NULL) {
            printf("You typed: '%s'\n", parts[i]);

            i++;
        }

        printf("done \n");

        if (strcmp(parts[0], "run") == 0) {
            struct Task tk = tasks[free_task_id];
            free_task_id++;
            run(&tk, parts);
        } else {
        if (strcmp(parts[0], "out") == 0) {
            out(parts);
        }

        }

        free_split_string(parts);

//    for (int i = 0; parts[i] != NULL; ++i)
//        free(parts[i]);
//    free(parts);


    free(buffer);

    ASSERT_SYS_OK(wait(NULL));

    return 0;
}
