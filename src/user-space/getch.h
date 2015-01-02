#ifndef USERSPACE_GETCH_H
#define USERSPACE_GETCH_H

#include <termios.h>

struct termios oldterm, newterm; 

void init_termios(int echo);
void reset_termios();
char getch();

#endif