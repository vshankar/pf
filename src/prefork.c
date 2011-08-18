#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <pthread.h>

#include "prefork.h"

extern void bitmap_set_bit_in_slot(cur_state *, int);
extern void bitmap_clear_bit_in_slot(cur_state *, int, int);

/*
 * after the child accept()s the connection; it checks if further request
 * can cause the client to wait. In such a case it will _tell_ the parent
 * to fork more processes
 */
void
child_init (int pid_slot, int listenfd, int wfd, cur_state *state)
{
    int acceptfd, len, nr_forks, n, next_slot;
    size_t readlen, writeln;
    char buffer[BUFSIZE];
    struct sockaddr_in clientaddr;

    len = sizeof(struct sockaddr_in);

    // TODO: lock around accept()

    while (1) {
        acceptfd = accept(listenfd, (struct sockaddr *) &clientaddr, &len);
        if (acceptfd < 0) { /* something went bad */
            perror("accept");
            break;
        }

        bitmap_clear_bit_in_slot(state, pid_slot, wfd);

        while (1) {
            readlen = read(acceptfd, buffer, BUFSIZE);

            if (readlen == 0)
                break;

            /* do some processing */
            /* ... */
            buffer[readlen] = '\0';
            writeln = write(acceptfd, buffer, strlen(buffer));
        }

        close(acceptfd);

        bitmap_set_bit_in_slot(state, pid_slot);
    }

    return;
}

int
init_state (cur_state **state)
{
    int i, ret = 0;
    cur_state *s = NULL;

    *state = (cur_state *) mmap(NULL, sizeof(cur_state), PROT_READ | PROT_WRITE,
                                MAP_ANON | MAP_SHARED, -1, 0);
    if (!*state)
        goto out;

    s = *state;
    bzero(s, sizeof(cur_state));

    for (i = 0; i < BITMAPSZ; i++)
        *(s->segment + i) = UINT_MAX;

    if (pthread_mutexattr_init(&s->attr) != 0) {
        perror("pthread_mutexattr_init");
        goto out;
    }

    pthread_mutexattr_setpshared(&s->attr, PTHREAD_PROCESS_SHARED);

    if (pthread_mutex_init(&s->mutex, &s->attr) != 0) {
        perror("pthread_mutex_init");
        goto out;
    }

    ret = 1;

 out:
    return ret;
}


/* needs to use fine grained locking */
void
fork_children(cur_state *state, void (*func)(int, int, int, cur_state *),
              int listenfd, int *pfd)
{
    int i;
    int child_count;

    pthread_mutex_lock(&state->mutex);
    {
        child_count = state->nr_forks;
        state->nr_forks += NFORKS;

        for (i = child_count; i < state->nr_forks; i++) {
            state->pid[i] = fork();
            if (state->pid[i] == 0) {
                close(pfd[0]);
                func(i, listenfd, pfd[1], state);
                exit(0);
            }

            // close(pfd[1]);
        }

        state->fork_cookie = 0;
    }
    pthread_mutex_unlock(&state->mutex);
}



int
main (int argc, char **argv)
{
    int i, listenfd;
    int pfd[2];
    cur_state *state = NULL;

    char rbuf;
    size_t n;
    struct sockaddr_in servaddr;

    if (!init_state(&state)) {
        printf("error initializing server state\n");
        exit(1);
    }

    /* TODO: setup signal handler */

    /* create parent-child communication pipe */
    if (pipe(pfd) < 0) {
        perror("pipe");
        exit(1);
    }

    /* create the listining socket */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("listenfd");
        exit(1);
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(LISTEN_PORT);

    if (bind(listenfd, (struct sockaddr *) &servaddr, sizeof(struct sockaddr_in)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 1024) < 0) {
        perror("listen");
        exit(1);
    }

    /* fork() off workers */
    fork_children(state, child_init, listenfd, pfd);

    while (1) {
        n = read(pfd[0], &rbuf, 1);
        if (n > 0) {
            fork_children(state, child_init, listenfd, pfd);
        }
    }

    /* parent continues here */
    pause();
}
