#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "getch.h"

#define MAX_QUERY_LEN 256

#define DELETE 127
#define BACKSPACE 8

int get_query();

int main()
{
    char query[MAX_QUERY_LEN];

    /* Enter REPL */
    while (1) {
        printf("kquery> ");

        query[0] = '\0';
        if (get_query(query, MAX_QUERY_LEN) == -1)
            break;
    }

    return 0;
}

/* Fill query string from stdin */
int get_query(char* query, size_t query_len)
{
    int i = 0;
    while (1) {
        char ch = getch();

        if (ch == '\n') {
            printf("\n   ...> ");
            continue;
        } else if (ch == ';') {
            printf(";\n");
            break;
        } else if (ch == DELETE || ch == BACKSPACE) {
            if (i != 0) {
                query[--i] = '\0';
                printf("\b\033[K"); // Backspace on terminal
            }
        } else {
            if (i < query_len) {
                query[++i] = ch;
                printf("%c", ch);
            }
        }
    }

    return feof(stdin) ? -1 : 0;
}