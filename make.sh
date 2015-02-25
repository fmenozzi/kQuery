#!/bin/bash

GREEN='\033[0;32m'
RED='\033[0;31m'
RESET='\033[0m'

sudo echo -e "${GREEN}Building module...${RESET}"
cd module
./unload.sh
if make; then
	echo -e "${GREEN}Done${RESET}"
	echo -e "${GREEN}Inserting module...${RESET}"
	if sudo insmod kquery_mod.ko; then
		echo -e "${GREEN}Done${RESET}"
		echo -e "${GREEN}Building kquery...${RESET}"
		cd ..
		if ./compile.sh; then
			echo -e "${GREEN}Done${RESET}"
		else
			echo -e "${RED}Failed to build kquery${RESET}"
		fi
	else
		echo -e "${RED}Failed to insert module${RESET}"
	fi
else
	echo -e "${RED}Failed to build module${RESET}"
fi
