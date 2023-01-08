#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "utils.h"
#include "err.h"

const int MAX_N_TASKS = 4096;
int free_task_id = 0;

struct Task {
    int id;
    int read_dsc_out;
    int read_dsc_err;
};

void run(struct Task* tk, char** parts) {
    const char* shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "executor_task_%d", tk->id);

    const int shm_size = 1024;

    int fd_memory = shm_open(shm_name, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
    ASSERT_SYS_OK(fd_memory);
    ASSERT_SYS_OK(ftruncate(fd_memory, shm_size));

    char* mapped_mem = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_memory, 0);
    printf("there %zd was: %s.end\n", strlen(mapped_mem), mapped_mem);

    pid_t pid;
    ASSERT_SYS_OK(pid = fork());

    if (!pid) {
        printf("Task %d started: pid %d.\n", 0, getpid());

        ASSERT_SYS_OK(dup2(fd_memory, STDOUT_FILENO));
//        ASSERT_SYS_OK(dup2(fd_memory, STDERR_FILENO));

//        exit(2137);
        ASSERT_SYS_OK(execvp(parts[1], parts+1));
    } else {
        tk->read_dsc_out = fd_memory;
//        tk->read_dsc_err = pipe_dsc_err[0];

        usleep(3000000);
        printf("there %zd was: %s.end\n", strlen(mapped_mem), mapped_mem);

        mapped_mem[9] = 'e';
        mapped_mem[10] = '\0';

        printf("there %zd was: %s.end\n", strlen(mapped_mem), mapped_mem);

    }

    ASSERT_SYS_OK(pid = fork());

    if (!pid) {
        printf("Task %d started: pid %d.\n", 0, getpid());

        ASSERT_SYS_OK(dup2(fd_memory, STDOUT_FILENO));
//        ASSERT_SYS_OK(dup2(fd_memory, STDERR_FILENO));

//        exit(2137);
        ASSERT_SYS_OK(execvp(parts[1], parts+1));
    } else {
        tk->read_dsc_out = fd_memory;
//        tk->read_dsc_err = pipe_dsc_err[0];

        usleep(3000000);
        printf("there %zd was: %s.end\n", strlen(mapped_mem), mapped_mem);

    }

//    ASSERT_SYS_OK(close(fd_memory));
//    ASSERT_SYS_OK(shm_unlink(shm_name));
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
            struct Task tk = tasks[free_task_id];
            free_task_id++;
            run(&tk, parts);
        } else {
        if (strcmp(parts[0], "out") == 0) {
            int num = atoi(parts[1]);

            out(&tasks[num]);
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
