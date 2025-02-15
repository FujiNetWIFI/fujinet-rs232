/* Contributed by fozztexx@fozztexx.com
 */

#include "parser.h"
#include <stdint.h>
#include <string.h>
#include <strings.h> // for strcasecmp()

#define DELIM " \t\r\n"

struct {
  const char *cmd;
  uint8_t token;
} commands[] = {
  {"ls", CMD_DIR},
  {"dir", CMD_DIR},
  {"get", CMD_GET},
  {"put", CMD_PUT},
  {"cd", CMD_CD},
  {"quit", CMD_EXIT},
  {"exit", CMD_EXIT},
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))
#define MAX_ARGS (sizeof(result.args) / sizeof(result.args[0]))

token get_token(const char *cmd)
{
  int idx;

  
  for (idx = 0; idx < NUM_COMMANDS; idx++)
    if (strcasecmp(cmd, commands[idx].cmd) == 0)
      return commands[idx].token;

  return CMD_UNKNOWN;
}

parsed parse_command(char *input)
{
  parsed result;
  char *token = strtok(input, DELIM);
  int idx;


  result.cmd = CMD_UNKNOWN;
  result.args[0] = input;
  result.args[1] = NULL;
  
  if (!token)
    return result;
  
  result.cmd = get_token(token);
  
  idx = 1;
  while ((token = strtok(NULL, DELIM)) != NULL && idx < MAX_ARGS - 1)
    result.args[idx++] = token;
  result.args[idx] = NULL;

  return result;
}
