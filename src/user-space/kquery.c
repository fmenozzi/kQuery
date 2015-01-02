#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "getch.h"

#define MAX_QUERY_LEN 256

#define DELETE 127
#define BACKSPACE 8

int  get_query();

void insert_into_str(char c, char* str, size_t pos, size_t max_len);
void backspace(char* str);

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

void insert_into_str(char c, char* str, size_t pos, size_t max_len)
{
    int char_fits = pos   < max_len;
    int null_fits = pos+1 < max_len;

    if (char_fits) {
        if (null_fits) {
            str[pos]   = c;
            str[pos+1] = '\0';
        } else {
            str[pos] = '\0';
        }
    } else {
        str[pos] = '\0';
    }
}

void backspace(char* str)
{
    str[strlen(str) - 1] = '\0';
}

/* Fill query string from stdin */
int get_query(char* query, size_t max_query_len)
{
    int i = 0;
    while (1) {
        char ch = getch();

        if (ch == '\n') {
            insert_into_str(' ', query, i++, max_query_len);
            printf("\n   ...> ");
            continue;
        } else if (ch == ';') {
            insert_into_str(';', query, i++, max_query_len);
            printf(";\n");
            break;
        } else if (ch == DELETE || ch == BACKSPACE) {
            if (i != 0) {
                backspace(query);
                printf("\b\033[K"); // Backspace on terminal
                i--;
            }
        } else {
            insert_into_str(ch, query, i++, max_query_len);
            printf("%c", ch);
        }
    }

    return feof(stdin) ? -1 : 0;
}