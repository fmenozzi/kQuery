/*
 * kQuery - Copyright (C) 2015
 *
 * Federico Menozzi <federicogmenozzi@gmail.com>
 * Halen Wooten     <halen+github@hpwooten.com>  
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by the 
 * Free Software Foundation; either version 2 of the License, 
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "deps/sqlite3.h"

#include "module/kquery_mod.h"

#define MAX_QUERY_LEN 512

#define CONTROL(x) ((x) & 0x1F)

#define DELETE 127
#define BACKSPACE 8

#define MAKE_GREEN "\e[32m"
#define MAKE_RED "\e[31m"
#define RESET_COLOR "\e[m"

//----------------------- INTERFACE TO KERNEL MODULE -----------------------//
//
/* Global vars for use in interface to module */
int fp;
char the_file[256] = "/sys/kernel/debug/";
char callbuf[MAX_CALL];  // Assumes no bufferline is longer
char respbuf[MAX_RESP];  // Assumes no bufferline is longer

/* Interface for performing "system calls" into kernel module */
int k_DoSyscall(char *call_string)
{
    int rc;

    strcpy(callbuf, call_string);

    rc = write(fp, callbuf, strlen(callbuf) + 1);
    if (rc == -1) {
        fprintf(stderr, MAKE_RED "Error writing %s\n" RESET_COLOR, the_file);
        return rc;
    }

    rc = read(fp, respbuf, sizeof(respbuf));
    if (rc == -1) {
        fprintf(stderr, MAKE_RED "Error reading %s\n" RESET_COLOR, the_file);
        return rc;
    }

    return rc;
}
//
//--------------------------------------------------------------------------//

//--------------------IMPLEMENTATION OF getch() ----------------------------//
//
int k_Getch()
{
    static struct termios oldterm, newterm;

    int ch;

    tcgetattr(0, &oldterm);
    newterm = oldterm;
    newterm.c_lflag &= ~ICANON;
    newterm.c_lflag &= ~ECHO;
    tcsetattr(0, TCSANOW, &newterm);

    ch = getchar();

    tcsetattr(0, TCSANOW, &oldterm);

    return ch == CONTROL('d') ? EOF : ch;
}
//
//--------------------------------------------------------------------------//

//------------------- ACQUIRING QUERY STRING FROM CONSOLE ------------------//
//
/* Insert c into str at index pos */
void k_InsertCharIntoStr(char c, char* str, size_t pos, size_t max_len)
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

void k_InsertStrIntoStr(char* suf, char* str, size_t pos, size_t max_len)
{
    int i, len = strlen(suf);
    for (i = 0; i < len; i++)
        k_InsertCharIntoStr(suf[i], str, pos++, max_len);
}

/* Remove last char in str */
void k_Backspace(char* str)
{
    str[strlen(str) - 1] = '\0';
}

/* Determine if char is valid in REPL */
int k_InvalidChar(char ch)
{
    // Letters and numbers are fine
    if (('A' <= ch && ch <= 'Z') ||
        ('a' <= ch && ch <= 'z') ||
        ('0' <= ch && ch <= '9'))
        return 0;

    // Check remaining possible valid chars
    char* valid = " @%*()_-+=<>,.?;'\"";
    int i, len = strlen(valid);
    for (i = 0; i < len; i++)
       if (ch == valid[i])
          return 0;
   return -1;
}

/* Fill query string from stdin */
int k_GetQueryFromStdin(char* query, size_t max_query_len)
{
    int i = 0, rc = 0;

    while (1) {
        int ch = k_Getch();
        if (ch == EOF) {
            rc = -1;
            break;
        }

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

            k_InsertCharIntoStr('\n', query, i++, max_query_len);
            fprintf(stdout, "\n   ...> ");

            continue;
        } else if (ch == BACKSPACE || ch == DELETE) {
            if (i != 0 && query[i-1] != '\n') {
                k_Backspace(query);
                fprintf(stdout, "\b\033[K");
                i--;
            }
        } else if (ch == '\t') {
            k_InsertCharIntoStr(' ', query, i++, max_query_len);
            fprintf(stdout, " ");
        } else {
            if (k_InvalidChar(ch))
                continue;

            k_InsertCharIntoStr(ch, query, i++, max_query_len);
            fprintf(stdout, "%c", ch);
        }
    }

    return rc;
}

/* Fill query string from shortened command line arg */
int k_GetQueryFromCommandLine(char* query, char* arg, size_t max_query_len)
{
    char* select_         = "SELECT ";
    char* select_distict_ = "SELECT DISTINCT ";
    char* from_           = "FROM ";
    char* where_          = "WHERE ";

    int ai, qi = 0, len = strlen(arg);
    for (ai = 0; ai < len; ai++) {
        if (arg[ai] == '@') {
            ai++;
            if (arg[ai] == 'S' || arg[ai] == 's') {
                if (arg[ai+1] == 'D' || arg[ai+1] == 'd') {
                    ai++;
                    k_InsertStrIntoStr(select_distict_, query, qi, max_query_len);
                    qi += strlen(select_distict_);
                } else {
                    k_InsertStrIntoStr(select_, query, qi, max_query_len);
                    qi += strlen(select_);
                }
            } else if (arg[ai] == 'F' || arg[ai] == 'f') {
                k_InsertStrIntoStr(from_, query, qi, max_query_len);
                qi += strlen(from_);
            } else if (arg[ai] == 'W' || arg[ai] == 'w') {
                k_InsertStrIntoStr(where_, query, qi, max_query_len);
                qi += strlen(where_);
            }
            ai++;
        } else {
            k_InsertCharIntoStr(arg[ai], query, qi++, max_query_len);
        }
    }
    return 1;
}
//
//--------------------------------------------------------------------------//

//--------------------------- DATABASE CALLBACKS ---------------------------//
//
/* Callback function for query execution in REPL*/
int k_QueryCallbackREPL(void *NotUsed, int argc, char **argv, char **azColName) 
{
    int i;
    for (i = 0; i < argc; i++) {
        fprintf(stdout, "%s", argv[i] ? argv[i] : "NULL");
        if (i != argc-1)
            fprintf(stdout, "|");
    }
    fprintf(stdout, "\n");

    return 0;
}

/* Callback function for query execution in pipeline */
int k_QueryCallbackPipeline(void *NotUsed, int argc, char **argv, char **azColName)
{
    int i;
    for (i = 0; i < argc; i++) {
        fprintf(stdout, "%s", argv[i] ? argv[i] : "NULL");
        if (i != argc-1)
            fprintf(stdout, "_");
    }
    fprintf(stdout, "\n");

    return 0;   
}
//
//--------------------------------------------------------------------------//

//------------------------------ SQLITE WRAPPERS ---------------------------//
//
/* Open database */
sqlite3* k_SQLiteOpen()
{
    sqlite3* db = NULL;
    int rc = sqlite3_open(NULL, &db);   // NULL filepath for in-memory database
    if (rc) {
        fprintf(stderr, MAKE_RED "Can't open kquery database: %s\n" RESET_COLOR, sqlite3_errmsg(db));
        sqlite3_close(db);
        close(fp);
        exit(-1);
    }
    return db;
}

/* Create Process table */
int k_CreateProcessTable(sqlite3* db)
{
    char* error_msg = NULL;
    char* create_stmt  = "CREATE TABLE IF NOT EXISTS process ("
                         "  pid        INT PRIMARY KEY,"
                         "  name       TEXT,"
                         "  parent_pid INT,"
                         "  state      BIGINT,"
                         "  flags      BIGINT,"
                         "  priority   INT,"
                         "  num_vmas   INT,"
                         "  total_vm   BIGINT"
                         ")";
    int rc = sqlite3_exec(db, create_stmt, NULL, 0, &error_msg);
    if (rc != SQLITE_OK) {
        fprintf(stdout, MAKE_RED "SQL error: %s\n" RESET_COLOR, error_msg);
        sqlite3_free(error_msg);
    }
    return rc;
}

/* Populate Process table */
int k_PopulateProcessTable(sqlite3* db)
{
    char* error_msg = NULL;
    int rc = k_DoSyscall("process_get_row");  // First call returns number of rows
    if (rc == -1)
        return rc;

    while (strcmp(respbuf, "")) {
        /* Fetch row */
        rc = k_DoSyscall("process_get_row");  // Subsequent calls fetch rows
        if (rc == -1)
            break;
        
        /* Insert row into table */
        rc = sqlite3_exec(db, respbuf, NULL, 0, &error_msg);
        if (rc != SQLITE_OK) {
            fprintf(stdout, MAKE_RED "SQL error: %s\n" RESET_COLOR, error_msg);
            sqlite3_free(error_msg);
        }
    }

    return rc;
}

/* Reset Process table */
int k_ResetProcessTable(sqlite3* db)
{
    char* error_msg = NULL;
    int rc = sqlite3_exec(db, "DELETE FROM process;", NULL, 0, &error_msg);
    if (rc != SQLITE_OK) {
        fprintf(stdout, MAKE_RED "SQL error: %s\n" RESET_COLOR, error_msg);
        sqlite3_free(error_msg);
    }
    return rc;
}

/* Execute query */
int k_ExecuteQuery(sqlite3* db, char* query, int(*callback)(void*, int, char**, char**))
{
    char* error_msg = NULL;
    int rc = sqlite3_exec(db, query, callback, 0, &error_msg);
    if (rc != SQLITE_OK) {
        fprintf(stdout, MAKE_RED "SQL error: %s\n" RESET_COLOR, error_msg);
        sqlite3_free(error_msg);
    }
    return rc;
}
//
//--------------------------------------------------------------------------//

int main(int argc, char* argv[])
{
    char query[MAX_QUERY_LEN];

    /* Open the file (module) */
    strcat(the_file, dir_name);
    strcat(the_file, "/");
    strcat(the_file, file_name);
    if ((fp = open(the_file, O_RDWR)) == -1) {
        fprintf(stderr, MAKE_RED "Error opening %s\n" RESET_COLOR, the_file);
        close(fp);
        exit(-1);
    }

    sqlite3* db = k_SQLiteOpen();

    if (argc == 2) {
        k_GetQueryFromCommandLine(query, argv[1], MAX_QUERY_LEN);

        k_CreateProcessTable(db);
        if (k_PopulateProcessTable(db) == SQLITE_OK)
            k_ExecuteQuery(db, query, k_QueryCallbackPipeline);
    } else if (argc == 1) {
        /* Enter REPL */
        while (1) {
            fprintf(stdout, "kquery> ");

            if (k_GetQueryFromStdin(query, MAX_QUERY_LEN) == -1)
                break;

            k_CreateProcessTable(db);
            if (k_PopulateProcessTable(db) == SQLITE_OK);
                k_ExecuteQuery(db, query, k_QueryCallbackREPL);
            k_ResetProcessTable(db);
        }
    } else {
        printf("Incorrect number of arguments\n");
    }

	/* Cleanup */
    sqlite3_close(db);
    close(fp);

    return 0;
}
