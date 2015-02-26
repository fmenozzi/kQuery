kQuery
======

A SQL interface to your Linux kernel. Think of it like a Linux-only [osquery](http://osquery.io), but with fewer and less-robust tables (but we don't require vagrant or VMs, so we got that going for us, which is nice).

## Building from Source
1. Building from source involves building the kernel module, loading it, and then building the user program. You can use the script `make.sh` to perform all three tasks at once, or you can use the `load.sh` and `compile.sh` scripts to perform the tasks separately. As a note, you may need to use the `chmod` command to make the scripts executable. 
2. To run the shell, execute:
        
          $ sudo ./kquery

## Current Features
  * `.quit` and `CTRL-D` to exit the shell
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
