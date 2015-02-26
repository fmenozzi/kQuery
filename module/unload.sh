#! /bin/bash

# Suppress output if files don't exist
sudo rmmod kquery_mod 2> /dev/null
rm kquery_mod.k*      2> /dev/null
rm kquery_mod.m*      2> /dev/null
rm kquery_mod.o       2> /dev/null
rm Module.symvers     2> /dev/null
rm modules.order      2> /dev/null

