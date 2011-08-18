#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <pthread.h>
#include <errno.h>

#include "prefork.h"

void
__bitmap_clear_bit_in_slot (cur_state *state, int pos, int notify_fd)
{
    int hops = 0;
    int shift = 0;
    int next_slot = 0;
    unsigned int *bitmap = state->segment;

    hops = pos / INTMUX;
    shift = pos % INTMUX;

#if 1
    printf("clearing slot:- hops: %d shifting: %d - value: %ld\n", hops, shift, *(bitmap + hops));
#endif

    *(bitmap + hops) &= ~(1 << shift);

    /* return the next available empty slot */
    next_slot = __bitmap_find_first_bit(bitmap);

    if ((next_slot >= state->nr_forks) && !state->fork_cookie) {
#if 1
        printf("[%d >= %d] notifying parent...\n", next_slot, state->nr_forks);
#endif
        if (write(notify_fd, "", 1) != 1)
            perror("write");

        state->fork_cookie = 1;
    }
}

void
__bitmap_set_bit_in_slot (cur_state *state, int pos)
{
    int hops = 0;
    int shift = 0;
    unsigned int *bitmap = state->segment;

    hops = pos / INTMUX;
    shift = pos % INTMUX;

#if 1
    printf("setting slot:- hops: %d shifting: %d - value: %ld\n", hops, shift, *(bitmap + hops));
#endif

    *(bitmap + hops) |= (1 << shift);
}

int
__bitmap_find_first_bit(unsigned int *bitmap)
{
    int i, idx = -1;

    for ( i = 0; i < BITMAPSZ; i++ ) {
        idx = ffs((int) bitmap[i]);
        if (--idx >= 0) {
            // idx = idx + (i * INTMUX);
            break;
        }
    }

    return idx;
}


/* thread safe routine */

void
bitmap_clear_bit_in_slot (cur_state *state, int pos, int notify_fd)
{
    pthread_mutex_lock(&state->mutex);
    {
        __bitmap_clear_bit_in_slot(state, pos, notify_fd);
    }
    pthread_mutex_unlock(&state->mutex);
}

void
bitmap_set_bit_in_slot (cur_state *state, int pos)
{
    pthread_mutex_lock(&state->mutex);
    {
        __bitmap_set_bit_in_slot(state, pos);
    }
    pthread_mutex_unlock(&state->mutex);
}

