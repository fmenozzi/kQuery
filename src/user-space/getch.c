#include "getch.h"

#include <stdio.h>

/* Initialize newterm terminal I/O settings */
void init_termios(int echo) {
    tcgetattr(0, &oldterm);                     // Grab oldterm terminal I/O settings 
    newterm = oldterm;                          // Make newterm settings same as oldterm settings
    newterm.c_lflag &= ~ICANON;                 // Disable buffered I/O
    newterm.c_lflag &= echo ? ECHO : ~ECHO;     // Set echo mode
    tcsetattr(0, TCSANOW, &newterm);            // Use these newterm terminal I/O settings now
}

/* Restore oldterm terminal I/O settings */
void reset_termios() {
    tcsetattr(0, TCSANOW, &oldterm);
}

/* Get char without echo */
char getch() {
    char ch;
    init_termios(0);    // Echo is off
    ch = getchar();
    reset_termios();
    return ch;
}