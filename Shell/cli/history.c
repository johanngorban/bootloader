#include "history.h"

#define HIST_TERM 3 // ASCII EndOfText

// Goto EOL+1
static void ring_history_skip_record(ring_history_t *hist, char **str_ptr)
{
    char *str = *str_ptr;
    char *phy_end = &hist->data[HISTORY_RING_LEN - 1];
    while (*str)
    {
        if (str == phy_end)
        {
            str = hist->data;
        }
        else
        {
            str++;
        }
    }
    if (str < phy_end)
    {
        str++;
    }
    *str_ptr = str;
}

static void ring_history_move_down(ring_history_t *hist, char **str_ptr)
{
    char *str = *str_ptr;
    char *phy_end = &hist->data[HISTORY_RING_LEN - 1];
    ring_history_skip_record(hist, &str);
    while (!(*str))
    {
        if (str == phy_end)
            str = hist->data;
        else
            str++;
        if (str == hist->last)
            break;
        if (str == hist->first)
            return; // on top of stack
    }
    *str_ptr = str;
}

static void ring_history_move_up(ring_history_t *hist, char **str_ptr)
{
    char *str = *str_ptr;
    char *phy_end = &hist->data[HISTORY_RING_LEN - 1];
    while (*str)
        str--; // to begin of old record
    while (!(*str))
    { // to end of new record
        if (str == hist->data)
            str = phy_end;
        else
            str--;
        if (str == hist->last)
            return; // bottom of stack
        if (str == hist->first)
            break;
    }
    while (*str)
        str--; // to begin of new record
    str++;
    *str_ptr = str;
}

static void ring_history_remove_last(ring_history_t *hist)
{
    char *line = hist->last;
    while (*line)
        line++;
    if (line != hist->last)
        line--;
    ring_history_move_up(hist, &hist->last); // move
    while (*line)
    {
        *line = 0;
        line--;
    } // erase
}

static char *ring_history_alloc_new(ring_history_t *hist, unsigned short size)
{
    char *str_begin;
    char *str_end;
    char *phy_end;
    if (size >= HISTORY_RING_LEN)
        return NULL;
    if (!hist->first && !hist->last)
    {                                 // first
        hist->first = &hist->data[1]; // the [0] always '0' !!!
        hist->last = hist->first;
    }
    else
    {
        phy_end = &hist->data[HISTORY_RING_LEN - 1];
        str_begin = hist->last;
        ring_history_skip_record(hist, &str_begin); // Go to EOL
        while (1)
        {
            str_end = str_begin + size;
            if (str_end > phy_end)
            {                                                // small size
                memset(str_begin, 0, (phy_end - str_begin)); // remove this record
                if (str_begin == &hist->data[1])
                    break;
                str_begin = &hist->data[1]; // reset
                continue;
            }
            if ((str_begin < hist->first && str_end > hist->first) ||
                str_begin == hist->first || str_end == hist->first)
            { // intersection with old begin pointer
                ring_history_skip_record(hist, &hist->first);
                memset(str_begin, 0, (hist->first - str_begin)); // remove old record
                if (!(*hist->first))
                {
                    hist->first = &hist->data[1];
                    if (str_begin == hist->first) // loop
                        break;
                }
                continue;
            }
            break;
        }
        hist->last = str_begin;
    }
    return hist->last;
}

void ring_history_add(ring_history_t *hist, char *str)
{
    int len = strlen(str);
    char *line = hist->last;
    if (hist->inactiv)
        return;
    if (len)
    {
        if (line && !strncmp(line, str, HISTORY_RING_LEN))
            return;
        len++; // for line terminator
        if ((line = ring_history_alloc_new(hist, len)))
        {
            memcpy(line, str, len);
            hist->last = line;
        }
    }
    hist->line = hist->last;
}

int ring_history_down(ring_history_t *hist, char *restored_line)
{
    char *line;
    if (!hist->last || hist->line == hist->last || hist->inactiv)
        return 0;
    line = hist->line;
    ring_history_move_down(hist, &line);
    if (line[0] == HIST_TERM)
        restored_line[0] = 0x00;
    else
        strcpy(restored_line, line);
    if (line == hist->last)
        ring_history_remove_last(hist);
    else
        hist->line = line;
    return 1;
}

int ring_history_up(ring_history_t *hist, char *restored_line)
{ // key UP process
    char *line;
    static char empty_dummy[] = {HIST_TERM, 0};
    if (!hist->last || hist->inactiv)
        return 0; // history is empty or inactiv
    line = hist->line;
    if (line == hist->last && line[0] != HIST_TERM)
    { // store backup
        ring_history_add(hist, restored_line[0] ? restored_line : empty_dummy);
    }
    else
    {
        if (line != hist->first)
            ring_history_move_up(hist, &line);
    }
    if (line != hist->first && strncmp(line, restored_line, HISTORY_RING_LEN) == 0)
        ring_history_move_up(hist, &line); // skip equal dublicate
    hist->line = line;
    if (!(*line))
        return 0;
    if (line[0] == HIST_TERM)
        restored_line[0] = 0x00;
    else
        strcpy(restored_line, line);
    return 1;
}

void ring_history_erase(ring_history_t *hist)
{
    memset(hist->data, 0, HISTORY_RING_LEN);
    hist->line = hist->data;
    hist->first = NULL;
    hist->last = NULL;
    hist->inactiv = 0; // Enabled by default
}

void ring_history_reset(ring_history_t *hist)
{
    char *line = hist->last;
    if (line && line[0] == HIST_TERM)
        ring_history_remove_last(hist);
    hist->line = hist->last;
}
