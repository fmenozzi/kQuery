kQuery
======

A SQL interface to your Linux kernel. Think of it like a Linux-only [osquery](http://osquery.io), but with fewer and less-robust tables (but we don't require vagrant or VMs, so we got that going for us, which is nice).

## Building from Source
Building from source involves building the kernel module, loading it, and then building the user program. You can use the script `make.sh` to perform all three tasks at once, or you can use the `load.sh` and `compile.sh` scripts to perform the tasks separately. As a note, you may need to use the `chmod` command to make the scripts executable. 
## Use
1. Use `sudo ./kquery` to run the shell
2. Use `sudo ./kquery "query"` to run individual queries

## Current Features
  * `.quit` and `CTRL-D` to exit the shell
  * Use in UNIX pipelines
      * When running a single query via command line, columns are separated by `_`
      * To help shorten command lengths, you can use the following `@` notation:
          * `@S`/`@s`   = `SELECT`
          * `@SD`/`@sd` = `SELECT DISTINCT`
          * `@F`/`@f`   = `FROM`
          * `@W`/`@w`   = `WHERE`
      * Results can be piped. For example, `sudo ./kquery "@S name @F Process" | sort` will print the names of all processes alphabetically
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
