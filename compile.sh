#!/bin/bash
gcc deps/sqlite3.c kquery.c -o kquery -ldl -lpthread
