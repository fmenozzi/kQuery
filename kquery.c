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

#include <ncurses.h>

#include "sqlite3.h"

#include "module/kquery_mod.h"

#define MAX_QUERY_LEN 512

#define CONTROL(x) ((x) & 0x1F)

#define K_PRINT(...) printw(__VA_ARGS__);refresh();

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
        K_PRINT("Error writing %s\n", the_file);
        return rc;
    }

    rc = read(fp, respbuf, sizeof(respbuf));
    if (rc == -1) {
        K_PRINT("Error reading %s\n", the_file);
        return rc;
    }

    return rc;
}
//
//--------------------------------------------------------------------------//

//------------------- ACQUIRING QUERY STRING FROM CONSOLE ------------------//
//
/* Insert c into str at index pos */
void k_InsertIntoStr(char c, char* str, size_t pos, size_t max_len)
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

/* Remove last char in str */
void k_Backspace(char* str)
{
    str[strlen(str) - 1] = '\0';
}

/* Fill query string from stdin */
int k_GetQuery(char* query, size_t max_query_len)
{
    int i = 0, rc = 0;

    attron(A_BOLD);
    while (1) {
        int ch = getch();
        if (ch == EOF || ch == CONTROL('d')) {
            rc = -1;
            break;
        }

        if (ch == '\n') {
            if (strcmp(query, ".quit") == 0) {
                rc = -1;
                K_PRINT("\n");
                break;
            }

            if (query[i-1] == ';') {
                K_PRINT("\n");
                break;
            }

            k_InsertIntoStr(' ', query, i++, max_query_len);
            K_PRINT("\n   ...> ");

            continue;
        } else if (ch == KEY_BACKSPACE) {
            if (i != 0) {
                k_Backspace(query);

                addch('\b');
                delch();
                refresh();

                i--;
            }
        } else {
            k_InsertIntoStr(ch, query, i++, max_query_len);
            K_PRINT("%c", ch);
        }
    }
    attroff(A_BOLD);

    return rc;
}
//
//--------------------------------------------------------------------------//

//--------------------------- DATABASE CALLBACKS ---------------------------//
//
/* Callback function for query execution */
int k_QueryCallback(void *NotUsed, int argc, char **argv, char **azColName) 
{
    int i;
    for (i = 0; i < argc; i++){
        K_PRINT("%s", argv[i] ? argv[i] : "NULL");
        if (i != argc-1)
            K_PRINT("|");
    }
    K_PRINT("\n");

    return 0;
}
//
//--------------------------------------------------------------------------//

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
    if ((fp = open(the_file, O_RDWR)) == -1) {
        fprintf(stderr, "Error opening %s\n", the_file);
        exit(-1);
    }

    /* Open database */
    rc = sqlite3_open(NULL, &db);   // NULL filepath creates an in-memory database
    if (rc) {
        fprintf(stderr, "Can't open kquery database: %s\n", sqlite3_errmsg(db));
        exit(-1);
    }

    /* Initialize ncurses */
    initscr();
    start_color();
    use_default_colors();
    init_pair(1, COLOR_GREEN, -1);
    keypad(stdscr, TRUE);
    cbreak();
    noecho();
    scrollok(stdscr, TRUE);

    /* Enter REPL */
    while (1) {
        /* Print prompt */
        attron(A_BOLD);
        attron(COLOR_PAIR(1));
        K_PRINT("kquery> ");
        attroff(A_BOLD);
        attroff(COLOR_PAIR(1));

        /* Get query from stdin */
        query[0] = '\0';
        if (k_GetQuery(query, MAX_QUERY_LEN) == -1)
            break;

        /* Create Process table (in case user does some stupid query to delete it) */
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
            K_PRINT("SQL error : %s\n", error_msg);
            sqlite3_free(error_msg);
        }

        /* Populate table */
        rc = k_DoSyscall("process_get_row");  // First call returns number of rows
        if (rc == -1)
            break;
        while (strcmp(respbuf, "")) {
            /* Fetch row */
            rc = k_DoSyscall("process_get_row");  // Subsequent calls fetch rows
            if (rc == -1)
                break;
            
            /* Insert row into table */
            rc = sqlite3_exec(db, respbuf, NULL, 0, &error_msg);
            if (rc != SQLITE_OK) {
                K_PRINT("SQL error: %s\n", error_msg);
                sqlite3_free(error_msg);
            }
        }

        /* Execute query */
        rc = sqlite3_exec(db, query, k_QueryCallback, 0, &error_msg);
        if (rc != SQLITE_OK) {
            K_PRINT("SQL error: %s\n", error_msg);
            sqlite3_free(error_msg);
        }

        /* Reset table */
        rc = sqlite3_exec(db, "DELETE FROM Process;", NULL, 0, &error_msg);
        if (rc != SQLITE_OK) {
            K_PRINT("SQL error: %s\n", error_msg);
            sqlite3_free(error_msg);
        }
    }

	/* Cleanup */
    sqlite3_close(db);
    close(fp);
	endwin();

    return 0;
}