#! /bin/bash

./unload.sh
make
sudo insmod kquery_mod.ko
