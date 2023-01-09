#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <pthread.h>

#include "utils.h"
#include "err.h"

const int MAX_N_TASKS = 4096;
int free_task_id = 0;

struct Task {
    int id;
    int read_dsc_out;
    int read_dsc_err;
    char* s_err;
    char* s_out;
    bool quit;
};

struct pair_task_bool {
    struct Task* task;
    bool is_out;
};

void* create_stream_process(void* data) {
    struct pair_task_bool* ptb = data;

    struct Task* tk = ptb->task;

    int read_dsc;
    char** s;

    if (ptb->is_out) {
        read_dsc = tk->read_dsc_out;
        s = &tk->s_out;
    } else {
        read_dsc = tk->read_dsc_err;
        s = &tk->s_err;
    }

    char* sm;
    size_t bufsize = 1024;

    sm = (char *)malloc(bufsize * sizeof(char));

    while (!tk->quit) {
        ssize_t nread = read(read_dsc, sm, 1024);
        ASSERT_SYS_OK(nread); // potrzeba mutexow

        if (nread == 0) {
            printf("nread == 0\n");
        }

        printf("i have sth to say: pid %zd, sentence %s.\n", nread, sm);

        *s = strdup(sm);
    }

    free(sm);
    return NULL;
}

void create_exec_process(char** parts, int* out, int* err) {
    pid_t pid;
    ASSERT_SYS_OK(pid = fork());

    if (!pid) {
        // Close the read descriptor.
        ASSERT_SYS_OK(close(out[0]));
        ASSERT_SYS_OK(close(err[0]));

        printf("Task %d started: pid %d.\n", 0, getpid());

        ASSERT_SYS_OK(dup2(out[1], STDOUT_FILENO));
        ASSERT_SYS_OK(dup2(err[1], STDERR_FILENO));

//        ASSERT_SYS_OK(close(out[1]));  // Close the original copy.
//        ASSERT_SYS_OK(close(err[1]));  // Close the original copy.

//        exit(2137);
        ASSERT_SYS_OK(execvp(parts[1], parts+1));
    } else {
        // Close the read descriptor.
    }
}

void run(struct Task* tk, char** parts) {
    tk->quit = false;
    tk->s_out = "";
    tk->s_err = "";

    int pipe_dsc_out[2], pipe_dsc_err[2];

    ASSERT_SYS_OK(pipe(pipe_dsc_out));
    ASSERT_SYS_OK(pipe(pipe_dsc_err));

    tk->read_dsc_out = pipe_dsc_out[0];
    tk->read_dsc_err = pipe_dsc_err[0];

    pthread_t out_thread, err_thread;
    struct pair_task_bool ptb_out, ptb_err;

    ptb_out.task = tk;
    ptb_out.is_out = true;
    pthread_create(&out_thread, NULL, create_stream_process, (void*)&ptb_out);

    ptb_err.task = tk;
    ptb_err.is_out = false;
    pthread_create(&err_thread, NULL, create_stream_process, (void*)&ptb_err);

    create_exec_process(parts, pipe_dsc_out, pipe_dsc_err);
//    create_stream_process(tk, pipe_dsc_out[0], false);
//    create_stream_process(tk, pipe_dsc_err[0], true);


//    ASSERT_ZERO(pthread_join(out_thread, NULL));
//    ASSERT_ZERO(pthread_join(err_thread, NULL));

//    ASSERT_SYS_OK(close(pipe_dsc[0]))
}

void out(struct Task* tk) {

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
            struct Task* tk = &tasks[free_task_id];
            free_task_id++;
            run(tk, parts);
        } else {
        if (strcmp(parts[0], "out") == 0) {
            int num = atoi(parts[1]);

            out(&tasks[num]);
        }

        }



        usleep(3000000);

        tasks[0].quit = true;

        printf("You typed: '%s'\n", tasks[0].s_out);
        printf("You typed: '%s'\n", tasks[0].s_err);


        free_split_string(parts);

//    for (int i = 0; parts[i] != NULL; ++i)
//        free(parts[i]);
//    free(parts);


    free(buffer);

    ASSERT_SYS_OK(wait(NULL));

    return 0;
}
