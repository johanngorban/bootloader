#ifndef __BOS_HISTORY_H
#define __BOS_HISTORY_H

#define HISTORY_RING_LEN 256

#ifndef NULL
#define NULL ((void *)0)
#endif

#if (HISTORY_RING_LEN > 0xFFF0)
#error "History lenght is out of limit! Maximum 0xFFF0."
#endif

typedef struct
{
    char data[HISTORY_RING_LEN];
    char *line; // current selected line
    char *first;
    char *last;
    unsigned char inactiv; // make 1 for disable history
} ring_history_t;

void ring_history_erase(ring_history_t *hist);
void ring_history_reset(ring_history_t *hist);
void ring_history_add(ring_history_t *hist, char *str);
int ring_history_up(ring_history_t *hist, char *restored_line);
int ring_history_down(ring_history_t *hist, char *restored_line);

#endif // HISTORY
