// kquery module
// Federico Menozzi and Halen Wooten

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include "kquery_mod.h"

char *respbuf; // Used to pass responses to calling processes

#define CWD_BUF_LEN 128

void process_get_row(char* buf)
{
    static int current_process = -1;    
    static int num_processes = 0;
    static struct task_struct** processes;    

    if (current_process == -1) {
        struct task_struct* task;
        int i = 0;
        
	// Count the number of processes       
        for_each_process(task)
            num_processes++;

        // Allocate memory for the process array.
        processes = kmalloc(num_processes * sizeof task, GFP_ATOMIC);
        
        // Add each task_struct to the array
        for_each_process(task) {
            processes[i] = task;
            i++;
        }

        // Put the number of tasks in the response buffer
        sprintf(buf, "%d", num_processes);

        // Once initialization is finished, set the current_process to the first one.
        current_process = 0;
    } else if (current_process < num_processes) {
        struct task_struct * task = processes[current_process];        

        // Fields to retrieve
        pid_t pid = pid_vnr(get_task_pid(task, PIDTYPE_PID));
        pid_t parent_pid = pid_vnr(get_task_pid(task->real_parent, PIDTYPE_PID));
        long state = task->state;
        unsigned int flags = task->flags;
        int prio = task->normal_prio;
        char comm[sizeof(current->comm)];

        get_task_comm(comm, task);

        // Put information in the response buffer
        sprintf(buf, "%d,%d,%ld,%x,%d,%s", pid, parent_pid, state, flags, prio, comm);
        current_process++;
    } else {
        // Reset data structures and clear out memory
        current_process = -1;
        num_processes = 0;

        // Put an empty string on the response buffer.
        strcpy(buf, "");

        kfree(processes);
        processes = NULL;
    }
}

static ssize_t kquery_call(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
    int check; // Need this for copy_from_user
    char callbuf[MAX_CALL];

    // Make sure module call doesn't exceed max length
    if (count >= MAX_CALL)
        return -EINVAL;
  
    preempt_disable();

    // Allocate memory for the response buffer
    respbuf = kmalloc(MAX_RESP, GFP_ATOMIC);
    if (respbuf == NULL) {
        preempt_enable(); 
        return -ENOSPC;
    }
    
    // Initialize response buffer
    strcpy(respbuf,"");

    if (strcmp(callbuf, "process_get_row")) {
        process_get_row(respbuf);
    }
   
    check = copy_from_user(callbuf, buf, count);
    callbuf[MAX_CALL - 1] = '\0'; // This now has the name of the function that the user called

    preempt_enable();
  
    *ppos = 0;  /* reset the offset to zero */
    return count;  /* write() calls return the number of bytes written */
}

static ssize_t kquery_return(struct file* file, char __user* userbuf,
                                size_t count, loff_t* ppos)
{
  int rc;

  preempt_disable();

  rc = strlen(respbuf) + 1; /* length includes string termination */

  if (count < rc) {
     respbuf[count - 1] = '\0';
     rc = copy_to_user(userbuf, respbuf, count);
  } else 
     rc = copy_to_user(userbuf, respbuf, rc);
 
  kfree(respbuf);
  respbuf = NULL;

  preempt_enable();

  *ppos = 0;  /* reset the offset to zero */
  return rc;  /* read() calls return the number of bytes read */
} 

static const struct file_operations my_fops = {
        .read = kquery_return,
        .write = kquery_call,
};

// The following functions setup the shared file for the module in debugfs.
struct dentry *dir, *file;

static int __init kquery_module_init(void)
{

  int file_value;
    
  dir = debugfs_create_dir(dir_name, NULL);
  if (dir == NULL) {
    printk(KERN_DEBUG "kquery: error creating %s directory\n", dir_name);
     return -ENODEV;
  }


  file = debugfs_create_file(file_name, 0666, dir, &file_value, &my_fops); //read-write by world
  if (file == NULL) {
    printk(KERN_DEBUG "kquery: error creating %s file\n", file_name);
     return -ENODEV;
  }

  printk(KERN_DEBUG "kquery: created new debugfs directory and file\n");

  return 0;
}

// Deletes shared file and clears memory
static void __exit kquery_module_exit(void)
{
  debugfs_remove(file);
  debugfs_remove(dir);
  if (respbuf != NULL)
     kfree(respbuf);
}

module_init(kquery_module_init);
module_exit(kquery_module_exit);
MODULE_LICENSE("GPL");
