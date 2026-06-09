#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLI task.
 *
 * Reads characters from the console, builds a command line buffer,
 * handles backspace editing, and executes commands when Enter is pressed.
 */
void cli_init(void);

#ifdef __cplusplus
}
#endif
