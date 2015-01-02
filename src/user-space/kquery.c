#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../deps/sqlite/sqlite3.h"

#include "getch.h"

#define MAX_QUERY_LEN 256

#define DELETE 127
#define BACKSPACE 8

int get_query();

void insert_into_str(char c, char* str, size_t pos, size_t max_len);
void backspace(char* str);

int main()
{
    sqlite3* db;
    char query[MAX_QUERY_LEN];
    int rc;
    char* create_stmt;
    char* error_msg = 0;

    /* Open database */
    rc = sqlite3_open("kquery.db", &db);
    if (rc) {
        fprintf(stderr, "Can't open kquery database: %s\n", sqlite3_errmsg(db));
        exit(0);
    }

    /* Create Process table */
    create_stmt = "CREATE TABLE IF NOT EXISTS Process ("
                  "  pid        INT PRIMARY KEY,"
                  "  name       TEXT,"
                  "  parent_pid INT,"
                  "  state      BIGINT,"
                  "  flags      BIGINT,"
                  "  priority   INT,"
                  "  num_vmas   INT,"
                  "  total_vm   BIGINT"
                  ")";
    rc = sqlite3_exec(db, create_stmt, NULL, 0, &error_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", error_msg);
        sqlite3_free(error_msg);
    }

    /* Enter REPL */
    while (1) {
        fprintf(stdout, "kquery> ");

        query[0] = '\0';
        if (get_query(query, MAX_QUERY_LEN) == -1)
            break;

        // THIS IS WHERE WE'D POPULATE THE DB

        /* Execute query */
        rc = sqlite3_exec(db, query, NULL, 0, &error_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", error_msg);
            sqlite3_free(error_msg);
        }

        /* Reset table */
        rc = sqlite3_exec(db, "DELETE FROM Process;", NULL, 0, &error_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", error_msg);
            sqlite3_free(error_msg);
        }
    }

    sqlite3_close(db);

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
            if (strcmp(query, ".quit") == 0) {
                fprintf(stdout, "\n");
                break;
            }

            insert_into_str(' ', query, i++, max_query_len);
            fprintf(stdout, "\n   ...> ");
            continue;
        } else if (ch == ';') {
            insert_into_str(';', query, i++, max_query_len);
            fprintf(stdout, ";\n");
            break;
        } else if (ch == DELETE || ch == BACKSPACE) {
            if (i != 0) {
                backspace(query);
                fprintf(stdout, "\b\033[K"); // Backspace on terminal
                i--;
            }
        } else {
            insert_into_str(ch, query, i++, max_query_len);
            fprintf(stdout, "%c", ch);
        }
    }

    // TODO: Recognize Ctrl-D as EOF and quit accordingly
    return strcmp(query, ".quit") == 0 ? -1 : 0;
}