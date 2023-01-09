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
int free_task_id = 0;

struct Task {
    int pid;
    int read_dsc_out;
    int read_dsc_err;
    char* s_err;
    char* s_out;
    bool quit;
    pthread_t out_thread;
    pthread_t err_thread;
    pthread_t close_thread;
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

    if (ptb->is_out) {
        read_dsc = tk->read_dsc_out;
        s = &tk->s_out;
    } else {
        read_dsc = tk->read_dsc_err;
        s = &tk->s_err;
    }
//    printf("wyszedlem\n");
    char* sm;
    size_t bufsize = 1024;

    sm = (char *)malloc(bufsize * sizeof(char));

    while (!tk->quit) {
        ssize_t nread = read(read_dsc, sm, 1024);
        ASSERT_SYS_OK(nread); // potrzeba mutexow

        if (nread == 0) {
//            printf("nread == 0\n");
            break;
        }

        sm[nread] = '\0';

//        printf("mowie: dlugosc %zd sentence:\n%s.\n", nread, sm);

        get_last_line(&sm);

        printf("sentence:\n%s.\n", sm);

        *s = strdup(sm);
    }

    free(sm);
//    printf("wyszedlem\n");

    return NULL;
}

void create_exec_process(char** parts, int* out, int* err) {
    printf("Task %d started: pid %d.\n", 0, getpid());
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

    int status = 77;
    int a = wait(NULL);

    pthread_mutex_lock(mutex);
    printf("Task %d ended: status %d.\n", a, status);
    pthread_mutex_unlock(mutex);

    tk->quit = true;
    close(tk->read_dsc_out); // nic nie robi?
    close(tk->read_dsc_err); // nic nie robi?

    pthread_join(tk->out_thread, NULL);
    pthread_join(tk->err_thread, NULL);

    printf("musze sie wykonac\n");

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

        ASSERT_SYS_OK(close(pipe_dsc_out[1]));  // Close the original copy.
        ASSERT_SYS_OK(close(pipe_dsc_err[1]));
    }

    pthread_create(&tk->close_thread, NULL, close_task, (void*)tk);
}

void out(struct Task* tk) {
    printf("You typed: '%s'\n", tk->s_out);
}

void err(struct Task* tk) {
    printf("You typed: '%s'\n", tk->s_err);
}

void clear_task(struct Task* tk) {
    pthread_cancel(tk->out_thread);
    pthread_cancel(tk->err_thread);
    pthread_cancel(tk->close_thread);

    kill(tk->pid, SIGKILL);
}

void quit(struct Task tasks[]) {
    for (int i = 0; i < free_task_id; i++) {
        clear_task(&tasks[i]);
    }

    printf("killed\n");
    exit(0);
}

int main() {
    struct Task tasks[MAX_N_TASKS];

    char *buffer;
    size_t bufsize = 512;
    buffer = (char *) malloc(bufsize * sizeof(char));

    while (true) {
        read_line(buffer, bufsize, stdin);

        if (strcmp(buffer, "\0") == 0) {
            break;
        }

        buffer[strcspn(buffer, "\n")] = 0;
        printf("Whole sentence: '%s'\n", buffer);

        char **parts = split_string(buffer);

        int i = 0;
        while (parts[i] != NULL) {
            printf("You typed: '%s'\n", parts[i]);

            i++;
        }

        if (strcmp(parts[0], "run") == 0) {
            struct Task *tk = &tasks[free_task_id];
            free_task_id++;
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

            pthread_cancel(tasks[t].out_thread);
            pthread_cancel(tasks[t].err_thread);
            pthread_cancel(tasks[t].close_thread);

            kill(tasks[t].pid, SIGINT);
        }

        if (strcmp(parts[0], "sleep") == 0) {
            int n = atoi(parts[1]);

            usleep(1000 * n);
        }

        if (strcmp(parts[0], "quit") == 0) {
            quit(tasks);
        }

        free_split_string(parts);

        pthread_mutex_unlock(mutex);
        pthread_mutex_lock(mutex);
    }

    free(buffer);
    quit(tasks);
//        usleep(5000000);




//        if (tasks[0].quit) {
//            printf("koniec, quit git.\n");
//        }
//        printf("koniec, %s\n", tasks[0].s_out);
//
//    if (tasks[1].quit) {
//        printf("koniec, quit git.\n");
//    }
//    printf("koniec, %s\n", tasks[1].s_out);


    return 0;
}


//void* sigchild(void* data) {
//    printf("tak");
//
//    int* dsc = data;
//
//    sigset_t mask;
//    int sfd;
//    struct signalfd_siginfo fdsi;
//    ssize_t s;
//
//    sigemptyset(&mask);
//    sigaddset(&mask, SIGCHLD);
//
//    printf("tak");
//
//    /* Block signals so that they aren't handled
//       according to their default dispositions. */
//
//    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
//        printf("sigprocmask");
//    printf("tak");
//
//    sfd = signalfd(-1, &mask, 0); //-1
//    if (sfd == -1)
//        printf("signalfd");
//
//    printf("czyta");
//    s = read(sfd, &fdsi, sizeof(fdsi));
//    if (s != sizeof(fdsi))
//        printf("read");
//    printf("tak");
//    if (fdsi.ssi_signo == SIGCHLD) {
//        printf("Got SIGCHLD\n");
//    } else {
//        printf("Read unexpected signal\n");
//    }
//    printf("tak");
//    return NULL;
//}


//        printf("Task %d parent: pid %d.\n", 0, getpid());
//        struct sigaction new_action;
//
//        sigemptyset(&new_action.sa_mask);
//        new_action.sa_flags = SA_NOCLDWAIT;
////        new_action.sa_handler = handler;
//        ASSERT_SYS_OK(sigaction(SIGCHLD, &new_action, NULL));
//
//        printf("Task %d parent: pid %d.\n", 0, getpid());
//
//        sigset_t signals_to_wait_for;
//        sigemptyset(&signals_to_wait_for);
//        sigaddset(&signals_to_wait_for, SIGCHLD);
//
//        printf("Task %d parent: pid %d.\n", 0, getpid());
//        ASSERT_SYS_OK(sigprocmask(SIG_BLOCK, &signals_to_wait_for, NULL));
//
//        printf("Task %d parent: pid %d.\n", 0, getpid());
//        siginfo_t info;
//        sigwaitinfo(&signals_to_wait_for, &info);
//        printf("Task %d parent: pid %d.\n", 0, getpid());
//
//        printf("Parent: got signal >>%s<< from %d\n", strsignal(info.si_signo), info.si_pid);
//
//        ASSERT_SYS_OK(sigprocmask(SIG_UNBLOCK, &signals_to_wait_for, NULL));

//void create_exec_process(char** parts, int* out, int* err) {
//    pid_t pid;
//    ASSERT_SYS_OK(pid = fork());
//
//    printf("Task %d started: pid %d.\n", 0, getpid());
//
//    if (!pid) {
//        ASSERT_SYS_OK(pid = fork());
//        if (!pid) {
//            // Close the read descriptor.
//            ASSERT_SYS_OK(close(out[0]));
//            ASSERT_SYS_OK(close(err[0]));
//
//
//
//            ASSERT_SYS_OK(dup2(out[1], STDOUT_FILENO));
//            ASSERT_SYS_OK(dup2(err[1], STDERR_FILENO));
//
//            ASSERT_SYS_OK(close(out[1]));  // Close the original copy.
//            ASSERT_SYS_OK(close(err[1]));  // Close the original copy.
//
//            ASSERT_SYS_OK(execvp(parts[1], parts+1));
//        } else {
//            int status = 77;
//            int a = wait(NULL);
//            printf("Task %zd ended: status %d.\n", pid, status); // ograniczony dostep
//
//            exit(0);
//        }
////        printf("Task %d parent: pid %s.\n", 0, parts[0]);
////        printf("Task %d parent: pid %s.\n", 0, parts[1]);
////        printf("Task %d parent: pid %s.\n", 0, parts[2]);
//
//    }
//}