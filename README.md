kQuery
======

A SQL interface to your Linux kernel. Think of it like a Linux-only [osquery](http://osquery.io), but with fewer and less-robust tables (but we don't require vagrant or VMs, so we got that going for us, which is nice).

## Building from Source
1. First, we have to load the kernel module. Navigate to the `module` directory and execute the following commands:

          $ make
          $ sudo insmod kquery_mod.ko
    
  Alternatively, the scripts `load.sh` (and `unload.sh`) are provided for your convenience.
    
2. Next, we need to compile the user program:

          $ gcc sqlite3.c kquery.c -o kquery -Wall -lpthread -ldl

3. To run the shell, execute:
        
          $ sudo ./kquery

## Current Features
  * `.quit` to exit the shell
  * The following tables:
      * **Process**
    
          Column     | Type
          ---------- | ----
          pid        | INT PRIMARY KEY
          name       | TEXT
          parent_pid | INT
          state      | BIGINT
          flags      | BIGINT
          priority   | INT
          num_vmas   | INT
          total_vm   | BIGINT

## Future Features
* Extra shell commands (`.tables`, `.schema`, etc.)
* More tables

## License
kQuery is licensed under the [GNU General Public License, version 2](https://www.gnu.org/licenses/gpl-2.0.html).
