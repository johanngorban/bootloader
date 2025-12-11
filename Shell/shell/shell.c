#include "shell.h"
#include "flash.h"
#include "cli.h"

void shell_init(void)
{
	cli_init();
}

void shell_handler(void)
{
	cli_handler();
}