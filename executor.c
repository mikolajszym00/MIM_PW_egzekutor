#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

//#include <sys/mman.h>
//#include <fcntl.h>
//#include <sys/stat.h>

#include <pthread.h>
#include <sys/wait.h>
//#include <sys/signalfd.h>
//#include <signal.h>

#include "utils.h"
#include "err.h"

const int MAX_N_TASKS = 4096;
int free_task_id = -1;

struct Task {
    int id;
    int pid;
    int read_dsc_out;
    int read_dsc_err;
    char* s_err;
    char* s_out;
    bool quit;
    pthread_t out_thread;
    pthread_t err_thread;
    pthread_t close_thread;

    pthread_mutex_t* mutex;
    pthread_mutex_t* mutex_out;
    pthread_mutex_t* mutex_err;
};

struct pair_task_bool {
    struct Task* task;
    bool is_out;
};

void get_last_line(char** msg) {
    char delim[] = "\n";

    char* ptr = strtok(*msg, delim);

    char* final_sentence = (char *)malloc(1024 * sizeof(char));

    while(ptr != NULL)
    {
        final_sentence = strdup(ptr);
//        printf("'%s'\n", ptr);
        ptr = strtok(NULL, delim);
    }

    *msg = strdup(final_sentence);
    free(final_sentence);
}

void* create_stream_process(void* data) {
    struct pair_task_bool* ptb = data;

    struct Task* tk = ptb->task;

    int read_dsc;
    char** s;
    pthread_mutex_t* mutex;

    if (ptb->is_out) {
        read_dsc = tk->read_dsc_out;
        s = &tk->s_out;
        mutex = tk->mutex_out;
    } else {
        read_dsc = tk->read_dsc_err;
        s = &tk->s_err;
        mutex = tk->mutex_err;
    }
//    printf("wyszedlem\n");
    char* sm;
    size_t bufsize = 1024;

    sm = (char *)malloc(bufsize * sizeof(char));

    while (true) {
        ssize_t nread = read(read_dsc, sm, 1024);
//        printf("pid: %d, me %d, nread %zd\n", tk->pid, tk->id, nread);
        ASSERT_SYS_OK(nread);

        if (nread == 0) {
//            printf("nread == 0\n");
            break;
        }

        sm[nread] = '\0';


        get_last_line(&sm);
//        printf("nazywam sie %d: '%s'.\n", tk->id, sm);

//        printf("sentence:\n%s.\n", sm);


        pthread_mutex_lock(mutex);
        *s = strdup(sm);
        pthread_mutex_unlock(mutex);
    }

    free(sm);
//    printf("wyszedlem\n");

    return NULL;
}

void create_exec_process(char** parts, int* out, int* err) {
//    printf("Task %d started: pid %d.\n", num, getpid());
//        usleep(2000000);
    // Close the read descriptor.
    ASSERT_SYS_OK(close(out[0]));
    ASSERT_SYS_OK(close(err[0]));

    ASSERT_SYS_OK(dup2(out[1], STDOUT_FILENO));
    ASSERT_SYS_OK(dup2(err[1], STDERR_FILENO));

    ASSERT_SYS_OK(close(out[1])); // Close the original copy.
    ASSERT_SYS_OK(close(err[1])); // Close the original copy.

//        set_close_on_exec(STDOUT_FILENO, true); // proba zamkniecia tych powyzej
//        set_close_on_exec(STDERR_FILENO, true);

    ASSERT_SYS_OK(execvp(parts[1], parts+1));
}

void* close_task(void* data) { // niech sam sie zabija
    struct Task* tk = data;

    int status;
//    printf("nazywam sie %d.\n", tk->id);
    ASSERT_SYS_OK(waitpid(tk->pid, &status, 0));
//    printf("nazywam sie %d.\n", tk->id);

    pthread_join(tk->out_thread, NULL);
    pthread_join(tk->err_thread, NULL);

    close(tk->read_dsc_out); // nic nie robi?
    close(tk->read_dsc_err); // nic nie robi?

    pthread_mutex_destroy(tk->mutex_out);
    pthread_mutex_destroy(tk->mutex_err);

    pthread_mutex_lock(tk->mutex);

//    printf("nazywam sie %d.\n", tk->id);
    if (WIFEXITED(status))
        printf("Task %d ended: status %d.\n", tk->id, WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        printf("Task %d ended: signalled.\n", tk->id);
    else
        printf("Task %d ended: unknown.\n", tk->id);

    pthread_mutex_unlock(tk->mutex);

    return NULL;
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

    struct pair_task_bool ptb_out, ptb_err;

    ptb_out.task = tk;
    ptb_out.is_out = true;
    pthread_create(&tk->out_thread, NULL, create_stream_process, (void*)&ptb_out);

    ptb_err.task = tk;
    ptb_err.is_out = false;
    pthread_create(&tk->err_thread, NULL, create_stream_process, (void*)&ptb_err);

    pid_t pid;
    ASSERT_SYS_OK(pid = fork());
    if (!pid) {
//        usleep(5000000);
        create_exec_process(parts, pipe_dsc_out, pipe_dsc_err);
    } else {
        tk->pid = pid;
        printf("Task %d started: pid %d.\n", tk->id, (int) pid);

        ASSERT_SYS_OK(close(pipe_dsc_out[1]));  // Close the original copy.
        ASSERT_SYS_OK(close(pipe_dsc_err[1]));
    }

    pthread_create(&tk->close_thread, NULL, close_task, (void*)tk);
}

void out(struct Task* tk) {
    pthread_mutex_lock(tk->mutex_out);
    printf("Task %d stdout: '%s'.\n", tk->id, tk->s_out);
    pthread_mutex_unlock(tk->mutex_out);
}

void err(struct Task* tk) {
    pthread_mutex_lock(tk->mutex_err);
    printf("Task %d stderr: '%s'.\n", tk->id, tk->s_err);
    pthread_mutex_unlock(tk->mutex_err);
}

void quit(struct Task tasks[]) {
    for (int i = 0; i <= free_task_id; i++) {
        kill(tasks[i].pid, SIGKILL);
    }

    for (int i = 0; i <= free_task_id; i++) {
//        printf("id: %d\n", tasks[i].id);
        pthread_join(tasks[i].close_thread, NULL);
//        printf("id: %d\n", tasks[i].id);
    }


//    printf("killed\n");
}

int main() {
    struct Task tasks[MAX_N_TASKS];

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_lock(&mutex);

    char *buffer;
    size_t bufsize = 512;
    buffer = (char *) malloc(bufsize * sizeof(char));

    while (true) {
        read_line(buffer, bufsize, stdin);

        if (strcmp(buffer, "\0") == 0) {
            break;
        }

        buffer[strcspn(buffer, "\n")] = 0;
//        printf("Whole sentence: '%s'\n", buffer);

        char **parts = split_string(buffer);

//        int i = 0;
//        while (parts[i] != NULL) {
//            printf("You typed: '%s'\n", parts[i]);
//
//            i++;
//        }

        if (strcmp(parts[0], "run") == 0) {
            free_task_id++;
            struct Task *tk = &tasks[free_task_id];
            pthread_mutex_t mutex_out;
            pthread_mutex_init(&mutex_out, NULL);
            tk->mutex_out = &mutex_out;

            pthread_mutex_t mutex_err;
            pthread_mutex_init(&mutex_err, NULL);
            tk->mutex_err = &mutex_err;

            tk->mutex = &mutex;
            tk->id = free_task_id;
            run(tk, parts);
        }

        if (strcmp(parts[0], "out") == 0) {
            int num = atoi(parts[1]);

            out(&tasks[num]);
        }

        if (strcmp(parts[0], "err") == 0) {
            int num = atoi(parts[1]);

            err(&tasks[num]);
        }

        if (strcmp(parts[0], "kill") == 0) {
            int t = atoi(parts[1]);

            kill(tasks[t].pid, SIGINT);
        }

        if (strcmp(parts[0], "sleep") == 0) {
            int n = atoi(parts[1]);

            usleep(1000 * n);
        }

        if (strcmp(parts[0], "quit") == 0) {
            free_split_string(parts);
            break;
        }

        free_split_string(parts);

//        usleep(1000000);
        pthread_mutex_unlock(&mutex);
        usleep(1000);
        pthread_mutex_lock(&mutex);
    }
    pthread_mutex_unlock(&mutex);
    free(buffer);
    quit(tasks);

    pthread_mutex_destroy(&mutex);

    return 0;
}
