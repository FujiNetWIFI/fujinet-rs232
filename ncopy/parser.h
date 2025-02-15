/* Contributed by fozztexx@fozztexx.com
 */

#ifndef _PARSER_H
#define _PARSER_H

typedef enum {
  CMD_UNKNOWN,
  CMD_DIR,
  CMD_GET,
  CMD_PUT,
  CMD_CD,
  CMD_EXIT,
} token;

typedef struct {
  token cmd;
  char *args[10];
} parsed;

extern parsed parse_command(char *input);

#endif /* _PARSER_H */
