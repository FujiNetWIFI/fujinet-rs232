/**
 * @brief quick and dirty graphics lib
 */

#ifndef GRLIB_H
#define GRLIB_H

int gr_mode(int mode);
void gr_pset(int x, int y, char c);
void gr_text(int x, int y, char *s);
void gr_color(char p, char c);
void gr_palette(unsigned char i, unsigned char c);

#endif /* GRLIB_H */
