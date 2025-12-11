#include "cli.h"
#include "config.h"
#include "history.h"
#include "flash.h"
#include "xmodem.h"
#include "rtc.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define ASCII_VT_ESC '!' // VT_ESC_START
#define ASCII_DEL 127    // Delete удаление

#define IS_ASCII_CTRL_CHAR(CH) (CH < ASCII_SPC || CH == ASCII_DEL)

#define VT_BS_SEQ "\033[D \033[D"
#define VT_SEEK_TO_BEGIN_SEQ "\033[0D\033[0C"
#define VT_INC_CURSOR_POS "\033[1C"
#define VT_DEC_CURSOR_POS "\033[1D"
#define VT_ERASE_FROM_CURSOR_POS "\033[K"
#define VT_CLS_LINE "\033[2K"     // Clear line
#define VT_CLS "\033[2J\033[1;1H" // CLS + move to up-left
#define VT_FMT_DEC_CURSOR_POS "\033[%dD"
#define VT_FMT_INC_CURSOR_POS "\033[%dC"
#define VT_ESC_BRACKET '['
#define VT_ESC_KEY_UP 'A'
#define VT_ESC_KEY_DOWN 'B'
#define VT_ESC_KEY_RIGHT 'C'
#define VT_ESC_KEY_LEFT 'D'
#define VT_ESC_OPERATIONAL 'O'
#define VT_ESC_KEY_DEL '3'
#define VT_ESC_KEY_HOME 'H' // 55d
#define VT_ESC_KEY_END 'F'  // 56d
#define VT_ESC_KEY_TILDA '~'
#define VT_ESC_OP_F1 '1'
#define VT_ESC_OP_F2 'Q'
#define VT_ESC_OP_F3 'R'
#define VT_ESC_OP_F4 'S'
#define VT_ESC_KEY_F1 '1'

#define IS_HIDDEN(ENT) (ENT->flags & CLI_FLG_HIDDEN)
#define IN_RUN_MODE(CLI) (CLI->mode == CONSOLE_MODE_READY)

#define LOG_MAX_TX 512

extern UART_HandleTypeDef CONSOLE_UART;

static char cli_rx_byte, log_tx_buf[LOG_MAX_TX];
static uint32_t cli_confirm_timer = 0;
cli_t console;

uint8_t cli_confirm(char *text);
void vt_draw_cmdline(cli_t *cli);

void cli_out(const char *fmt, ...)
{
    va_list arp;
    va_start(arp, fmt);
    uint16_t count = vsnprintf((char *)log_tx_buf, sizeof(log_tx_buf), fmt, arp);
    if (count > (LOG_MAX_TX - 100))
    {
        sprintf(log_tx_buf, CRLF "[CLI] tx buf is big" CRLF);
        count = 18;
    }
    HAL_UART_Transmit(&CONSOLE_UART, (uint8_t *)log_tx_buf, count, HAL_MAX_DELAY);
    va_end(arp);
}

MENU_DECLARE(bulat, 0, PLATFORM_NAME, "Embedded Bootloader shell", "'?' - for list of commands", 0)
{
    return CLI_SUCCESS;
}

MENU_DECLARE_STATIC(info, 0, "info for software", NULL, 0)
{
    uint8_t minor, major, patch;
    uint32_t crc;
    util_get_ver(FLASH_ADDR_APP, APP_SIZE, &major, &minor, &patch, &crc);
    cli_out("current software    ver: %d.%d.%d crc: 0x%x" CRLF, major, minor, patch, crc);
    util_get_ver(FLASH_ADDR_DISK, APP_SIZE, &major, &minor, &patch, &crc);
    cli_out("upload software     ver: %d.%d.%d crc: 0x%x" CRLF, major, minor, patch, crc);
    return CLI_SUCCESS;
}

MENU_DECLARE(xmodem, 0, "xmodem", "get file by xmodem protocol", "description", 0)
{
    cli_out("Begin the Xmodem transfer now..." CRLF);
    return CLI_GET_XMODEM;
}

MENU_DECLARE(update, 0, "update", "update software", "", 0)
{
    uint8_t minor, major, patch, minor_n, major_n, patch_n;
    uint32_t crc;
    util_get_ver(FLASH_ADDR_APP, APP_SIZE, &major, &minor, &patch, &crc);
    util_get_ver(FLASH_ADDR_DISK, APP_SIZE, &major_n, &minor_n, &patch_n, &crc);

    if (major >= major_n && minor >= minor_n && patch >= patch_n)
    {
        cli_out("update software ver: %d.%d.%d to ver: %d.%d.%d" CRLF, major, minor, patch, major_n, minor_n, patch_n);
        if (!cli_confirm("update?"))
        {
            return CLI_SUCCESS;
        }
        cli_out(CRLF);
    }
    flash_result_t res = flash_copy(FLASH_ADDR_DISK, FLASH_ADDR_APP, APP_SIZE);
    cli_out(CRLF);
    return CLI_SUCCESS;
}

MENU_DECLARE(out, 0, "out", "exit bootloader", "", 0)
{
    NVIC_SystemReset();
    return CLI_SUCCESS;
}

MENU_DECLARE(gop, 0, "gop", "go to program", "", 0)
{
    flash_jump_to_app();
    return CLI_SUCCESS;
}

void cli_init(void)
{
    console.root = CLI_ENTRY(bulat);
    MENU_LINK_ROOT(&console, info);
    MENU_LINK_ROOT(&console, xmodem);
    MENU_LINK_ROOT(&console, update);
    MENU_LINK_ROOT(&console, out);
		MENU_LINK_ROOT(&console, gop);

    vt_draw_cmdline(&console);
}

uint8_t cli_confirm(char *text)
{
    cli_out("%s ", text);
    while (1)
    {
        if (cli_confirm_timer++ > 0xFFFFFF)
        {
            cli_confirm_timer = 0;
            cli_out(CRLF);
            return 0;
        }
        if (HAL_UART_Receive(&CONSOLE_UART, (uint8_t *)&cli_rx_byte, 1, 0) == HAL_OK)
        {
            if (cli_rx_byte == 0x6e)
            {
                cli_confirm_timer = 0;
                cli_out("n", CRLF);
                return 0;
            }
            else if (cli_rx_byte == 0x79)
            {
                cli_confirm_timer = 0;
                cli_out("y", CRLF);
                return 1;
            }
        }
    }
    return 0;
}

uint8_t cli_get_next_word_offset(char *str)
{
    uint8_t off = 0;
    while (str[off] != ASCII_NUL && str[off] != ASCII_SPC)
    {
        off++;
    }
    while (str[off] == ASCII_SPC)
    {
        off++;
    }
    return off;
}

/*
 */
void str_and_str(char *buf, char *str, unsigned char limit)
{
    if (buf && str)
    {
        while (limit && (*buf == *str))
        {
            str++;
            buf++;
            limit--;
        }
    }
    if (*buf != *str) //@todo ?
    {
        *buf = 0;
    }
}

/*
 */
int cli_str_is_prototype(cli_t *cli, char *prot, char *find)
{
    uint8_t c;
    if (!prot || !find)
    {
        return -1;
    }
    for (c = 0; prot[c] == find[c] && prot[c] && find[c]; c++)
    {
    }
    if (c)
    {
        if (prot[c] == ASCII_NUL && (find[c] == ASCII_NUL || find[c] == ASCII_SPC))
        {
            return 2;
        }
    }
    else
    {
        return -1;
    }
    if (prot[c] != find[c] && prot[c] && find[c] && find[c] != ASCII_SPC)
    {
        return -1;
    }
    if (prot[c] == ASCII_NUL && find[c] != ASCII_SPC)
    {
        return -1;
    }
    return 1;
}

int cli_get_complete_count(cli_t *cli, cli_entry_t *entry, char *key_pattern, char exclude_global)
{
    int cnt = 0;
    int fnd_res;
    if (!exclude_global)
    {
        cnt += cli_get_complete_count(cli, cli->root->next, key_pattern, 1);
    }
    for (; entry; entry = entry->next)
    {
        if ((fnd_res = cli_str_is_prototype(cli, entry->prototype, key_pattern)) > 0)
        {
            if (IS_HIDDEN(entry))
            {
                if (IN_RUN_MODE(cli) && (fnd_res == 2))
                {
                    cnt++;
                }
            }
            else
            {
                cnt++;
                if (fnd_res == 2)
                {
                    return cnt;
                }
            }
        }
    }
    return cnt;
}

cli_entry_t *cli_get_complete_by_num(cli_t *cli, cli_entry_t *cli_entry, char *key_pattern, char exclude_global, unsigned char num)
{
    unsigned char cnt = 0;
    unsigned char depth;
    int fnd_res;
    cli_entry_t *entry;
    if (exclude_global)
    {
        // cli_out("[cli] execute global\r\n");
        depth = 1;
        entry = cli_entry;
    }
    else
    {
        // cli_out("[cli] global->next\r\n");
        depth = 2;
        entry = cli->root->next; // globals //global->
    }
    while (depth)
    {
        for (; entry; entry = entry->next)
        {
            if ((fnd_res = cli_str_is_prototype(cli, entry->prototype, key_pattern)) > 0)
            {
                if (IS_HIDDEN(entry))
                {
                    if (IN_RUN_MODE(cli) && (fnd_res == 2))
                    {
                        cnt++;
                    }
                }
                else
                {
                    cnt++;
                }
                if (cnt == num)
                {
                    return entry;
                }
            }
        }
        entry = cli_entry;
        depth--;
    }
    return NULL;
}

cli_result_t cli_get_expanded_entry(cli_t *cli, cli_entry_t **found_candidate)
{
    int wbegin = 0;
    int expanded_cnt = 0;
    int fnd_cnt;
    int num;
    char common_mask[24]; // maximum mask size
    cli_entry_t *entry = cli->root;
    char *line = cli->cmdline;
    *found_candidate = NULL;
    cli->args_offset = 0;
    cli->mask_len = 0;
    if (*line == 0)
    {
        return CLI_NOT_FOUND;
    }
    while (entry)
    {
        fnd_cnt = cli_get_complete_count(cli, entry->child, &line[wbegin], expanded_cnt);
        switch (fnd_cnt)
        {
        case 0:
            // проверка прав доступа
            if (expanded_cnt && entry->access <= cli->access)
            {
                while (line[wbegin] == ASCII_SPC)
                {
                    wbegin++;
                }
                cli->args_offset = wbegin;
                cli->mask_len = strlen(entry->prototype);
                *found_candidate = entry;
                return CLI_SUCCESS;
            }
            return CLI_NOT_FOUND;
            break;
        case 1: // only one match - can expand
            entry = cli_get_complete_by_num(cli, entry->child, &line[wbegin], expanded_cnt, 1);
            wbegin += cli_get_next_word_offset(&line[wbegin]);
            expanded_cnt++;
            break;
        default: // multiple match - incorrect (but we can select the candidate)
            common_mask[0] = 0;
            for (num = 1; num <= fnd_cnt; num++)
            {
                *found_candidate = cli_get_complete_by_num(cli, entry->child, &line[wbegin], expanded_cnt, num);
                if (common_mask[0])
                {
                    str_and_str(common_mask, (*found_candidate)->prototype, sizeof(common_mask));
                }
                else
                {
                    strncpy(common_mask, (*found_candidate)->prototype, sizeof(common_mask));
                }
            }
            cli->mask_len = strlen(common_mask);
            cli->args_offset = wbegin + cli_get_next_word_offset(&line[wbegin]);
            return CLI_AMBIGUOUS;
            break;
        }
    }
    return CLI_INTERNAL_ERROR;
}

void cli_decode_args(cli_t *cli)
{
    char blk = 0;
    uint8_t offset = cli->args_offset;
    cli->argc = 0;
    while (offset < cli->cmdlen)
    {
        if (blk)
        { // skip data block
            if (cli->cmdline[offset] == blk)
            {
                if (blk == ' ')
                {
                    cli->cmdline[offset] = 0;
                }
                blk = 0;
            }
        }
        else
        {
            switch (cli->cmdline[offset])
            {
            case '"':
                blk = cli->cmdline[offset];
                if (cli->argc < CONSOLE_MAX_ARGS)
                {
                    cli->argv[cli->argc++] = &cli->cmdline[offset];
                }
                break;
            case '\'':
                blk = cli->cmdline[offset];
                if (cli->argc < CONSOLE_MAX_ARGS)
                {
                    cli->argv[cli->argc++] = &cli->cmdline[offset];
                }
                break;
            case '[':
                blk = ']';
                if (cli->argc < CONSOLE_MAX_ARGS)
                {
                    cli->argv[cli->argc++] = &cli->cmdline[offset];
                }
                break;
            case '{':
                blk = '}';
                if (cli->argc < CONSOLE_MAX_ARGS)
                {
                    cli->argv[cli->argc++] = &cli->cmdline[offset];
                }
                break;
            case '(':
                blk = ')';
                if (cli->argc < CONSOLE_MAX_ARGS)
                {
                    cli->argv[cli->argc++] = &cli->cmdline[offset];
                }
                break;
            case ' ':
                // blk = ' ';
                cli->cmdline[offset] = 0;
                break;
            default:
                if (cli->argc < CONSOLE_MAX_ARGS)
                {
                    cli->argv[cli->argc++] = &cli->cmdline[offset];
                }
                blk = ' ';
                break;
            }
        }
        offset++;
    }
}

void cli_encode_args(cli_t *cli)
{
    int offset = cli->args_offset;
    while (offset < cli->cmdlen)
    {
        if (cli->cmdline[offset] == ASCII_NUL)
        {
            cli->cmdline[offset] = ASCII_SPC;
        }
        offset++;
        if (cli->argc)
        {
            cli->argv[cli->argc--] = NULL;
        }
    }
}

void cli_entry_print_list(cli_t *cli, cli_entry_t *entry_list)
{
    unsigned char plen;         // prototype len int
    unsigned char max_plen = 0; // max of prototype len int
    char out_fmt[16];
    cli_entry_t *entry;
    // get maximal length of prototype
    for (entry = entry_list; entry; entry = entry->next)
    {
        if (!IS_HIDDEN(entry) && cli->access >= entry->access)
        {
            if ((plen = strlen(entry->prototype)) > max_plen)
            {
                max_plen = plen;
            }
        }
    }
    // Generate format "\r\n %-Ns : %s"
    snprintf(out_fmt, sizeof(out_fmt), CRLF " %%-%ds : %%s", max_plen);
    for (entry = entry_list; entry; entry = entry->next)
    {
        if (!IS_HIDDEN(entry) && cli->access >= entry->access)
        {
            cli_out(out_fmt, entry->prototype, entry->desc ? entry->desc : "---");
        }
    }
}

cli_result_t cli_entry_print_usage_dynamic(cli_t *cli, cli_entry_t *entry)
{
    if (entry)
    {
        if (entry->desc)
        {
            cli_out("%s : %s", entry->prototype, entry->desc);
        }
        if (entry->usage)
        {
            cli_out(CRLF " %s", entry->usage);
        }
        if (entry->child)
        { // we a have childs
            cli_out(CRLF);
            cli_entry_print_list(cli, entry->child);
        } /* else {
             if (entry->usage)
                 cli_out("\r\n %s", entry->usage);
         }*/
        cli_out(CRLF);
        return CLI_SUCCESS;
    }
    return CLI_NOT_FOUND;
}

cli_result_t cli_entry_exec(cli_t *cli, cli_entry_t *entry)
{
    cli_result_t res = CLI_SUCCESS;
    if (!entry->exec)
    {
        return CLI_ERR_DAMAGED_ENTRY;
    }
    if (entry->flags & CONSOLE_FLG_NO_EXEC)
    {
        cli_entry_print_usage_dynamic(cli, entry);
        return CLI_ERR_DUMMY;
    }

    cli_decode_args(cli);
    if (cli->argc && (entry->flags & CLI_FLG_NO_ARGS))
    {
        res = CLI_ERR_ARGS_NOT_SUPPORTED;
    }
    else
    {
        res = entry->exec(cli);
    }
    cli_encode_args(cli);

    switch (res)
    {
    case CLI_GET_XMODEM:
        cli->mode = CONSOLE_MODE_XMODEM;
        break;
    case CLI_GET_HIDDEN_LINE:
        cli->mode = CONSOLE_MODE_HIDDEN_LINE;
        break;
    case CLI_CLEAR_TERMINAL:
        cli_out(VT_CLS);
        return CLI_SUCCESS;
    default:
        break;
    }
    return res;
}

cli_result_t cli_exec_line(cli_t *cli)
{
    cli_entry_t *completed;
    cli_result_t res = cli_get_expanded_entry(cli, &completed);
    if (res == CLI_SUCCESS && completed)
    {
        if ((res = cli_entry_exec(cli, completed)) == CLI_ERROR)
        {
            if (completed->flags & CONSOLE_FLG_NO_ERROR)
            {
                return CLI_SUCCESS;
            }
        }
    }
    return res;
}

void vt_puts(cli_t *cli, char *str)
{
    cli_out(VT_ERASE_FROM_CURSOR_POS);
    while (*str)
    {
        cli_out("%c", *str);
        str++;
    }
}

static void vt_move_cursor_relatively(cli_t *cli, int count)
{
    if (count > 0)
    {
        cli_out(VT_FMT_INC_CURSOR_POS, count);
    }
    else if (count < 0)
    {
        cli_out(VT_FMT_DEC_CURSOR_POS, -(count));
    }
}

void cli_insert_char(cli_t *cli, char ch)
{
    if (cli->cmdlen >= CONSOLE_MAX_LINE_LEN)
    {
        return;
    }
    uint8_t pos = cli->cmdlen + 1;
    while (pos > cli->cmdpos)
    { // Memory move to right
        cli->cmdline[pos] = cli->cmdline[pos - 1];
        pos--;
    }
    cli->cmdline[pos] = ch;
    cli->cmdlen++;
    cli->cmdpos++;
    // Update terminal
    if (cli->cmdlen == cli->cmdpos)
    {
        cli_out("%c", ch);
    }
    else
    {
        vt_puts(cli, &cli->cmdline[cli->cmdpos - 1]);
        vt_move_cursor_relatively(cli, cli->cmdpos - cli->cmdlen);
    }
}

static void cli_delete_char(cli_t *cli)
{
    uint8_t pos;
    if (cli->cmdlen)
    {
        pos = cli->cmdpos;
        while (pos < cli->cmdlen)
        { // Memory move to left
            cli->cmdline[pos] = cli->cmdline[pos + 1];
            pos++;
        }
        cli->cmdline[pos] = 0;
        cli->cmdlen--;
    }
}

void cli_backspace_char(cli_t *cli)
{
    if (cli->cmdpos)
    {
        cli_out(VT_BS_SEQ); // remove char + automatic decrement position
        cli->cmdpos--;      // remove left char of current position
        cli_delete_char(cli);
        if (cli->cmdpos != cli->cmdlen)
        {
            vt_puts(cli, &cli->cmdline[cli->cmdpos]);
            vt_move_cursor_relatively(cli, cli->cmdpos - cli->cmdlen);
        }
    }
}

/**
 * Метод выводит стороку приглащения на ввод
 */
void vt_draw_cmdline(cli_t *cli)
{
    cli_out(VT_CLS_LINE "\r%s%c ", PLATFORM_NAME, '#');
    for (char c = 0; c < cli->cmdlen; c++)
    {
        if (cli->cmdline[c])
        {
            cli_out("%c", cli->cmdline[c]); //@todo
        }
    }
    vt_move_cursor_relatively(cli, cli->cmdpos - cli->cmdlen); // Sync cursor position
}

void cli_reset_cmdline(cli_t *cli)
{
    uint8_t i;
    for (i = 0; i < sizeof(cli->cmdline); i++)
    {
        cli->cmdline[i] = 0;
    }
    for (i = 0; i < CONSOLE_MAX_ARGS; i++)
    {
        cli->argv[i] = NULL;
    }
    cli->argc = cli->cmdpos = cli->cmdlen = cli->escape = cli->mask_len = cli->args_offset = 0;
}

cli_result_t cli_process_res(cli_t *cli, cli_result_t res)
{
    switch (res)
    {
    case CLI_MSG_DONE:
        cli_out("%% Done\n");
        cli_reset_cmdline(cli);
        break;
    case CLI_MSG_UNK_NUM_FMT:
        cli_out("%% Unknown number format entered\n");
        cli_reset_cmdline(cli);
        break;
    case CLI_ERROR:
        cli_reset_cmdline(cli);
        return res;
    case CLI_SUCCESS:
        cli_reset_cmdline(cli);
        break;
    case CLI_EXIT:
        return res;
    case CLI_TERMINATE:
        return res;
    case CLI_READY:
        return res;
    case CLI_INTERNAL_ERROR:
        cli_out("%% Internal BUG!\n");
        break;
    case CLI_AMBIGUOUS:
        cli_out("%% Ambiguous command\n");
        break;
    case CLI_NOT_FOUND:
        cli_out("%% Unrecognized command\n");
        break;
    case CLI_ERR_MANY_ARGS:
        cli_out("%% Too many arguments\n");
        break;
    case CLI_ERR_FEW_ARGS:
        cli_out("%% Too few arguments\n");
        break;
    case CLI_ERR_ARGS_NOT_SUPPORTED:
        cli_out("%% Arguments is not supported!\n");
        break;
    case CLI_PRINT_HELP:
        break;
    case CLI_ERR_DUMMY:
        cli_reset_cmdline(cli);
        break;
    case CLI_ERR_DAMAGED_ENTRY:
        cli_out("%% Fatal error, the command is damaged!\n");
        break;
    default:
        cli_out("%% Unknown error %d [BUG]\n", res);
        break;
    }
    return CLI_SUCCESS;
}

cli_result_t cli_process_escape(cli_t *cli, char ch)
{
    switch (cli->escape)
    {
    case VT_ESC_OP_F1:
    {
        // cli_out(CRLF "[CLI]VT_ESC_OP_F1 %x %c" CRLF, ch, ch);
        cli->escape = NULL;
        if (ch == VT_ESC_KEY_TILDA)
        {
            return CLI_UART_MODE;
        }
        break;
    }
    case ASCII_ESC:
    {
        // cli_out("ASCII_ESC ch: %x" CRLF, ch);
        if (ch == VT_ESC_BRACKET)
        {
            cli->escape = VT_ESC_BRACKET; // rewrite ESC mode force
            return CLI_SUCCESS;
        }
        break;
    }
    case VT_ESC_BRACKET:
    {
        switch (ch)
        {
        case VT_ESC_KEY_DEL:
        {
            cli->escape = VT_ESC_KEY_DEL;
            break;
        }
        case VT_ESC_KEY_LEFT:
        {
            cli->escape = 0;
            if (cli->cmdpos)
            {
                cli->cmdpos--;
                cli_out("%s", VT_DEC_CURSOR_POS);
            }
            break;
        }
        case VT_ESC_KEY_RIGHT:
        {
            cli->escape = NULL;
            if (cli->cmdpos < cli->cmdlen)
            {
                cli->cmdpos++;
                cli_out("%s", VT_INC_CURSOR_POS);
            }
            break;
        }
        case VT_ESC_KEY_UP:
        {
            if (ring_history_up(&cli->history, cli->cmdline))
            {
                cli_out(VT_ERASE_FROM_CURSOR_POS);
                cli->cmdlen = strlen(cli->cmdline);
                cli->cmdpos = cli->cmdlen;
                vt_draw_cmdline(cli);
            }
            cli->escape = NULL;
            break;
        }
        case VT_ESC_KEY_DOWN:
        {
            if (ring_history_down(&cli->history, cli->cmdline))
            {
                cli_out(VT_ERASE_FROM_CURSOR_POS);
                cli->cmdlen = strlen(cli->cmdline);
                cli->cmdpos = cli->cmdlen;
                vt_draw_cmdline(cli);
            }
            cli->escape = NULL;
            break;
        }
        case VT_ESC_OP_F1:
        {
            cli->escape = VT_ESC_OP_F1;
            break;
        }
        default:
            cli->escape = NULL;
            // cli_out(CRLF "[CLI] not found code %d %x %c" CRLF, ch, ch, ch);
        }
        break;
    }
    case VT_ESC_KEY_DEL:
    {
        cli->escape = NULL;
        if (ch != VT_ESC_KEY_TILDA)
        {
            return CLI_ERROR;
        }
        if (cli->cmdpos != cli->cmdlen)
        {
            cli_delete_char(cli);
            vt_puts(cli, &cli->cmdline[cli->cmdpos]);
            vt_move_cursor_relatively(cli, cli->cmdpos - cli->cmdlen);
        }
        break;
    }
    }
    return CLI_SUCCESS;
}

cli_result_t cli_print_usage_dynamic(cli_t *cli)
{
    cli_entry_t *completed;
    switch (cli_get_expanded_entry(cli, &completed))
    {
    case CLI_AMBIGUOUS:
        completed = (completed->parent) ? completed->parent : cli->root;
    case CLI_SUCCESS:
        if (!IS_HIDDEN(completed))
        {
            cli_out(CRLF);
            cli_entry_print_usage_dynamic(cli, completed);
        }
        else
        {
            return CLI_NOT_FOUND;
        }
        break;
    default:
        cli_out(CRLF);
        cli_entry_print_usage_dynamic(cli, cli->root);
        cli_out(CRLF);
        break;
    }
    return CLI_SUCCESS;
}

cli_result_t cli_process_char(cli_t *cli, char ch)
{
    cli_result_t res = CLI_SUCCESS;

    if (cli->escape)
    {
        res = cli_process_escape(cli, ch);
        if (res == CLI_INTERNAL_ERROR)
        {
            vt_draw_cmdline(cli);
            res = CLI_SUCCESS;
        }
        else if (res == CLI_UART_MODE)
        {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, 1);
            cli_out(CRLF "[\x1b[32mINFO\x1b[39m] Switch console CP to MC" CRLF);
            vt_draw_cmdline(cli);
            res = CLI_SUCCESS;
        }
        return res;
    }
    else if (ch == ASCII_ESC)
    {
        cli->escape = ASCII_ESC;
    }
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15) == 0)
    {
        return CLI_SUCCESS;
    }
    switch (ch)
    {
    case '?': // Print usage for current mode
        if (cli_print_usage_dynamic(cli) == CLI_SUCCESS)
        {
            vt_draw_cmdline(cli);
        }
        break;
    case ASCII_HT: // TAB
        // res = __cli_try_auto_complete(cli);
        //  maybe now convert cmdline to argv??
        // if (res == CLI_NOT_FOUND && cb_tab)
        //((func_t)cb_tab)(cli);
        break;
    case ASCII_CR:
    case ASCII_LF: // EMIT
        // cli_out_putc('\r');
        cli_out(CRLF);
        ring_history_reset(&cli->history);
        ring_history_add(&cli->history, cli->cmdline);
        switch ((res = cli_exec_line(cli)))
        {
        case CLI_GET_XMODEM:
        case CLI_GET_HIDDEN_LINE:
            res = CLI_SUCCESS;
            cli_reset_cmdline(cli);
            break;
        default:
            res = cli_process_res(cli, res);
            vt_draw_cmdline(cli);
            break;
        }
        return res;
        break;
        // case ASCII_CR:
        // cli_out("\n\nUNEXPECTED TERMINAL INTERRUPT: 0x%02X\n", ch);
        //     return CLI_SUCCESS;//CLI_ERROR;
        //     break;
        /*
case ASCII_SOH:  // ^A
    //cli_out("%s",VT_SEEK_TO_BEGIN_SEQ);
    //uscli.cmdpos = 0;
    break;
case ASCII_STX:
    break;
    */
    case ASCII_BS: // Backspace!
    case ASCII_DEL:
    {
        cli_backspace_char(cli);
        break;
    }
    case ASCII_SPC:
    {
        cli_insert_char(cli, ch);
        break;
    }
    default:
        // ignore spaces/comment at begin of line
        if ((ch == ASCII_SPC || ch == '#') && (cli->cmdpos == 0))
        {
            break;
        }
        if (IS_ASCII_CTRL_CHAR(ch))
        {
            // cli_out(CRLF "TERMINAL ERROR CODE: %d" CRLF, ch);
            return CLI_SUCCESS;
        }
        cli_insert_char(cli, ch);
        break;
    }
    if (!cli->escape)
    {
        ring_history_reset(&cli->history);
    }
    return CLI_SUCCESS;
}

cli_result_t cli_readline_hidden(cli_t *cli, char ch)
{
    if (ch == ASCII_CR || ch == ASCII_LF)
    {
        if (cli->cmdlen)
        {
            cli->cmdline[cli->cmdlen] = 0; // terminate
            return CLI_SUCCESS;
        }
    }
    else
    {
        if (IS_ASCII_CTRL_CHAR(ch))
            return CLI_NOT_FOUND;
        if (cli->cmdpos < sizeof(cli->cmdline))
        {
            cli->cmdline[cli->cmdpos++] = ch;
            cli->cmdlen = cli->cmdpos;
        }
    }
    return CLI_NOT_FOUND;
}

cli_result_t cli_handler(void)
{
    cli_result_t res = CLI_SUCCESS;
    if (console.mode == CONSOLE_MODE_XMODEM)
    {
        xmodem_result_t res = xmodem_handler();
        if (res == XMODEM_OK || res == XMODEM_CANCEL)
        {
            if (res == XMODEM_CANCEL)
            {
                cli_out(CRLF "xmodem exit..." CRLF);
            }
            console.mode = CONSOLE_MODE_READY;
            cli_reset_cmdline(&console);
            vt_draw_cmdline(&console);
        }
        return CLI_SUCCESS;
    }
    if (HAL_UART_Receive(&CONSOLE_UART, (uint8_t *)&cli_rx_byte, 1, 0) != HAL_OK)
    {
        return CLI_SUCCESS;
    }
    res = cli_process_char(&console, cli_rx_byte);
    return res;
}

static int menu_check_entry(cli_entry_t *entry)
{
    char *str;
    if (entry && entry->prototype)
    {
        for (str = entry->prototype; *str; str++)
        {
            if (*str == ASCII_SPC)
            {
                return 0;
            }
        }
    }
    return 1;
}

cli_result_t menu_is_linked(cli_entry_t *level, cli_entry_t *entry)
{
    while (level)
    {
        if (level == entry)
        {
            return CLI_SUCCESS;
        }
        level = level->next;
    }
    return CLI_ERROR;
}

static cli_entry_t *menu_entry_sort_list(cli_entry_t *head_entry)
{
    cli_entry_t *old_head, *new_head, *p, *pr, *head;
    head = head_entry;
    new_head = NULL;
    while (head)
    {
        old_head = head;
        head = head->next;
        for (p = new_head, pr = NULL; p && strncmp(old_head->prototype, p->prototype, CONSOLE_MAX_LINE_LEN) > 0; pr = p, p = p->next)
        {
            if (p == p->next)
            {
                p->next = NULL; // FIXME - it is incorrect
            }
        }
        if (pr)
        {
            old_head->next = p;
            pr->next = old_head;
        }
        else
        {
            old_head->next = new_head;
            new_head = old_head;
        }
    }
    return new_head;
}

cli_result_t menu_link_entry(cli_t *cli, cli_entry_t *parent, cli_entry_t *cli_entry)
{
    if (menu_check_entry(cli_entry))
    {
        if (!parent)
        {
            parent = cli->root; //->root
        }
        if (menu_is_linked(parent->child, cli_entry) == CLI_SUCCESS)
        {
            return CLI_SUCCESS; // dublicate ignore
        }
        if (parent->child)
        {
            for (parent = parent->child; parent->next; parent = parent->next)
            {
            }
            if (parent != cli_entry)
            {
                parent->next = cli_entry;
                cli_entry->parent = parent->parent;
            }
        }
        else
        {
            parent->child = cli_entry;
            cli_entry->parent = parent;
        }
        cli_entry->parent->child = menu_entry_sort_list(cli_entry->parent->child);
        return CLI_SUCCESS;
    }
    else
    {
        cli_out("[CLI]: Entry error - '%s'" CRLF, cli_entry->prototype);
    }
    return CLI_ERROR;
}

cli_result_t menu_link_root(cli_t *cli, cli_entry_t *cli_entry)
{
    return menu_link_entry(cli, cli->root, cli_entry);
}