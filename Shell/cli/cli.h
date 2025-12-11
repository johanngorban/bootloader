#ifndef __BOS_CLI_H
#define __BOS_CLI_H

#include "stm32f1xx.h"
#include "history.h"

#define CLI_MAX_TX 512
#define CONSOLE_MAX_ARGS 8
#define CONSOLE_MAX_LINE_LEN 80 // Max 254
#define CRLF "\r\n"

enum
{
    ASCII_NUL, // ^@ 0  Null
    ASCII_SOH, // ^A 1  Start of Heading (?????? ?????????)
    ASCII_STX, // ^B 2  Start of Text
    ASCII_ETX, // ^C 3  End of Text
    ASCII_EOT, // ^D 4  END OF TRANSMISSION ????? ????????
    ASCII_ENQ, // ^E 5  Enquiry (?????? ????:"??? ???")
    ASCII_ACK, // ^F 6  Acknowledge (????????????? ? ??????)
    ASCII_BEL, // ^G 7  Bell (??????)
    ASCII_BS,  // ^H 8  Backspace (??? ?? ?? ??????????)
    ASCII_HT,  // ^I 9  Horizontal Tabulation (Tab ?? ??????????)
    ASCII_LF,  // ^J 10 Line Feed (??????? ??????)  (Ctrl+M <---> Enter)
    ASCII_VT,  // ^K 11 Vertical Tabulation
    ASCII_FF,  // ^L 12 Form Feed (??????? ????????)
    ASCII_CR,  // ^M 13 Carriage Return (??????? ???????)  (Ctrl+M <---> Enter)
    ASCII_SO,  // ^N 14 Shift Out (???????????? ?? ??????? ????? ????????)
    ASCII_SI,  // ^O 15 Shift In (???????????? ?? ??????????? ????? ????????)
    ASCII_DLE, // ^P 16 Data Link Escape. (???????????? ?????? ??????) (Shift ?? ??????????)
    ASCII_DC1, // ^Q 17 Device control 1 (Ctrl ?? ??????????)
    ASCII_DC2, // ^R 18 Device control 2 (Alt ?? ??????????)
    ASCII_DC3, // ^S 19 Device control 3
    ASCII_DC4, // ^T 20 Device control 4 (CapsLock ?? ??????????)
    ASCII_NAK, // ^U 21 Negative Acknowledgement (???????????????)
    ASCII_SYN, // ^V 22 Synchronous/Idle (c????????????  (???????? ??? ???????? ??????????)).
    ASCII_ETB, // ^W 23 End of Transmission Block (????? ????? ????????, ??? ????????? ????????)
    ASCII_CAN, // ^X 24 Cancel (??????). ????????????? ?????????? ??????.
    ASCII_EM,  // ^Y 25 End of Medium. (?????????? ????? ????????)
    ASCII_SUB, // ^Z 26 Substitute (???????????). ??????? ???????.
    ASCII_ESC, // ^[ 27 Escape (??????????). ???. ??? ?????? ??????????? ???????????????????
    ASCII_FS,  // ^\ 28 File Separator
    ASCII_GS,  // ^] 29 Group Separator
    ASCII_RS,  // ^^ 30 Record Separator
    ASCII_US,  // ^_ 31 Unit Separator
    ASCII_SPC  //    32 Space - ??????
};

typedef enum
{
    CLI_FLG_HIDDEN = (1 << 0),       // + hidden command (no auto complete, no help, etc)
    CLI_FLG_ALWAYS_EXEC = (1 << 1),  // execute if group container (ignore CLI_FLG_NO_EXEC)
    CLI_FLG_NO_ARGS = (1 << 2),      // + error if args detected
    CLI_FLG_COMMON_ARGS = (1 << 3),  // disable process args by US-CLI-engine
    CLI_FLG_DISABLE = (1 << 4),      // disable command processing in runtime
    CONSOLE_FLG_NO_ERROR = (1 << 5), // + ignore CLI_ERROR result (replace on CLI_SUCCESS when exec)
    CLI_FLG_USE_CONFIG = (1 << 6),   // entry used in running/startup config
    CLI_FLG_PROC = (1 << 7),         // + the entry execute as procedure (switch root) (ignore by group entry)
    CONSOLE_FLG_NO_EXEC = (1 << 8),  // запрящает запуск, но есть вывод инфы при интаре
    CLI_FLG_NO_SWITCH = (1 << 9),    // запрящает переключаться в рут режим
    CLI_FLG_ALIAS = (1 << 10),       // +/- is cmd is ALI - need always set this flags
} cli_flags_t;

typedef enum
{
    CLI_SUCCESS = 0,
    CLI_INTERNAL_ERROR,
    CLI_ERROR,
    CLI_AMBIGUOUS,
    CLI_EXIT,        // close CLI shell application
    CLI_TERMINATE,   // close CLI shell with parent application (but not last)
    CLI_USAGE_ERROR, // for automatic print usage
    CLI_GO_BACK,     // exit from current mode
    CLI_READY,       // when application switched
    CLI_NOT_FOUND,
    CLI_GET_HIDDEN_LINE, // read password from terminal
    CLI_CLEAR_TERMINAL,
    CLI_UART_MODE,

    // Print
    CLI_PRINT_HELP,
    CLI_PRINT_CHILD_TREE,
    CLI_PRINT_DESC,
    CLI_PRINT_CMDLINE,   // force redraw current cmdline
    CLI_PRINT_MODE_HELP, // print help for current mode after execute user CB
    CLI_PRINT_HISTORY,   // print current cli history

    CLI_ERR_DUMMY,
    CLI_ERR_MANY_ARGS,
    CLI_ERR_FEW_ARGS,
    CLI_ERR_ARGS_NOT_SUPPORTED,
    CLI_ERR_DAMAGED_ENTRY,
    CLI_MSG_DONE,
    CLI_MSG_UNK_NUM_FMT,

    CONSOLE_XMODEM,
    CLI_GET_XMODEM
} cli_result_t;

typedef enum
{
    CONSOLE_MODE_READY,       // +
    CONSOLE_MODE_HIDDEN_LINE, // +
    CONSOLE_MODE_XMODEM,      // +
} cli_mode_t;

typedef struct cli_entry_struct cli_entry_t;

typedef struct cli_struct
{
    char cmdline[CONSOLE_MAX_LINE_LEN];
    unsigned char cmdlen;
    unsigned char cmdpos;
    cli_entry_t *root;
    char *argv[CONSOLE_MAX_ARGS];
    unsigned char argc;
    char escape;
    cli_mode_t mode;
    unsigned char args_offset;
    unsigned char mask_len;
    unsigned char access;
    ring_history_t history;
} cli_t;

struct cli_entry_struct
{
    char *prototype; // command line prototype format
    char *desc;
    char *usage;
    cli_result_t (*exec)(cli_t *cli);
    cli_entry_t *next;   // next on current level of tree
    cli_entry_t *parent; // parent group
    cli_entry_t *child;  // child level
    cli_flags_t flags;   // entry flags
    unsigned char access;
};

#define CLI_ENTRY(name) (&name##_menu_entry)

#define MENU_DECLARE_STATIC(cmd_name, acs, dsc, usg, flg)                                                                                                    \
    static cli_result_t cmd_name##_exec_cb(cli_t *cli);                                                                                                      \
    static cli_entry_t cmd_name##_menu_entry = {.prototype = #cmd_name, .desc = dsc, .usage = usg, .exec = cmd_name##_exec_cb, .flags = flg, .access = acs}; \
    static cli_result_t cmd_name##_exec_cb(__attribute__((unused)) cli_t *cli)

#define MENU_DECLARE(cmd_name, acs, proto, dsc, usg, flg)                                                                                         \
    cli_result_t cmd_name##_exec_cb(cli_t *cli);                                                                                                  \
    cli_entry_t cmd_name##_menu_entry = {.prototype = proto, .desc = dsc, .usage = usg, .exec = cmd_name##_exec_cb, .flags = flg, .access = acs}; \
    cli_result_t cmd_name##_exec_cb(__attribute__((unused)) cli_t *cli)

#define MENU_LINK_ROOT(CLI, slave) menu_link_root(CLI, &slave##_menu_entry)
#define MENU_LINK(CLI, master, slave) menu_link_entry(CLI, &master##_menu_entry, &slave##_menu_entry)

void menu_init(cli_t *cli);
cli_result_t menu_link_root(cli_t *cli, cli_entry_t *cli_entry);
cli_result_t menu_link_entry(cli_t *cli, cli_entry_t *parent, cli_entry_t *cli_entry);
cli_result_t menu_link_global(cli_t *cli, cli_entry_t *cli_entry);

void cli_init(void);
cli_result_t cli_handler(void);
void cli_set_root(void);
void cli_out(const char *fmt, ...);
void cli_debug(const char *fmt, ...);

#endif // CLI
