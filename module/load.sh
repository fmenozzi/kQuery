#! /bin/bash

./unmake.sh
make
sudo insmod kquery_mod.ko
