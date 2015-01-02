#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../../deps/sqlite/sqlite3.h"

#include "../kernel-space/kquery_mod.h"

#include "getch.h"

#define MAX_QUERY_LEN 256

#define DELETE 127
#define BACKSPACE 8

int fp;
char the_file[256] = "/sys/kernel/debug/";
char callbuf[MAX_CALL];  // Assumes no bufferline is longer
char respbuf[MAX_RESP];  // Assumes no bufferline is longer

void do_syscall(char *call_string);

int insert_callback(void *NotUsed, int argc, char **argv, char **azColName);

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

    /* Open the file (module) */
    strcat(the_file, dir_name);
    strcat(the_file, "/");
    strcat(the_file, file_name);
    if ((fp = open (the_file, O_RDWR)) == -1) {
        fprintf(stderr, "Error opening %s\n", the_file);
        exit(-1);
    }

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

        /* Get query from stdin */
        query[0] = '\0';
        if (get_query(query, MAX_QUERY_LEN) == -1)
            break;

        /* Populate table */
        do_syscall("process_get_row");  // First call returns number of rows
        while (strcmp(respbuf, "")) {
            /* Fetch row */
            do_syscall("process_get_row");  // Subsequent calls fetch rows
            
            /* Insert row into table */
            rc = sqlite3_exec(db, respbuf, NULL, 0, &error_msg);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "SQL error: %s\n", error_msg);
                sqlite3_free(error_msg);
            }
        }

        /* Execute query */
        rc = sqlite3_exec(db, query, insert_callback, 0, &error_msg);
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

    close(fp);

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
    // TODO: Recognize Ctrl-D as EOF and quit accordingly

    int i = 0, rc = 0;
    while (1) {
        char ch = getch();

        if (ch == '\n') {
            if (strcmp(query, ".quit") == 0) {
                rc = -1;
                fprintf(stdout, "\n");
                break;
            }

            if (query[i-1] == ';') {
                fprintf(stdout, "\n");
                break;
            }

            insert_into_str(' ', query, i++, max_query_len);
            fprintf(stdout, "\n   ...> ");
            continue;
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

    return rc;
}

int insert_callback(void *NotUsed, int argc, char **argv, char **azColName) {
    int i;
    for (i = 0; i < argc; i++){
        fprintf(stdout, "%s", argv[i] ? argv[i] : "NULL");
        if (i != argc-1)
            fprintf(stdout, "|");
    }
    printf("\n");
    return 0;
}

void do_syscall(char *call_string)
{
    int rc;

    strcpy(callbuf, call_string);

    rc = write(fp, callbuf, strlen(callbuf) + 1);
    if (rc == -1) {
        fprintf(stderr, "error writing %s\n", the_file);
        fflush(stderr);
        exit (-1);
    }

    rc = read(fp, respbuf, sizeof(respbuf));
    if (rc == -1) {
        fprintf(stderr, "error reading %s\n", the_file);
        fflush(stderr);
        exit (-1);
    }
}