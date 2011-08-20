#ifndef __PREFORK_H
#define __PREFORK_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>

#define INTMUX  (sizeof(unsigned int) * 8) // may be use unsigned long later

#define NCLIENTS  100
#define NFORKS    5

#define BUFSIZE   4096
#define BITMAPSZ  1024 / INTMUX

#define LISTEN_PORT  6007

typedef struct {
    pid_t pid[NCLIENTS];
    int nr_forks;
    int fork_cookie;
    unsigned int segment[BITMAPSZ];
    pthread_mutexattr_t attr;
    pthread_mutex_t mutex;
} cur_state;

#endif
