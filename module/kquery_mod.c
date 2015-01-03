// kquery module
// Federico Menozzi and Halen Wooten

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include "kquery_mod.h"

char *respbuf;

/*
 * Returns insert command with appropriate values for Process table
 */
static void process_get_row(char *buf)
{
	static int current_process = -1;
	static int num_processes = 0;
	static struct task_struct **processes;

	if (current_process == -1) {
		struct task_struct *task;
		int i = 0;

		for_each_process(task)
			num_processes++;

		processes = kmalloc(num_processes * sizeof(task), GFP_ATOMIC);

		for_each_process(task)
			processes[i++] = task;

		sprintf(buf, "%d", num_processes);

		current_process = 0;
	} else if (current_process < num_processes) {
		struct task_struct *task = processes[current_process];

        	/* Fields to retrieve */
		pid_t pid = pid_vnr(get_task_pid(task, PIDTYPE_PID));
		pid_t parent_pid = pid_vnr(get_task_pid(task->real_parent, 
							PIDTYPE_PID));
		long state = task->state;
		unsigned int flags = task->flags;
		int prio = task->normal_prio;
		char comm[sizeof(current->comm)];

		struct mm_struct *mm;
		int num_vmas = 0;
		unsigned long total_vm = 0;

		get_task_comm(comm, task);

		mm = task->mm;    

		if (mm != NULL) {
			down_read(&mm->mmap_sem);
			num_vmas = mm->map_count;
                	total_vm = mm->total_vm;
                	up_read(&mm->mmap_sem);
                }

                sprintf(buf, 
                "INSERT INTO Process VALUES (%d,'%s',%d,%ld,%u,%d,%d,%lu);", 
							pid, 
							comm, 
							parent_pid, 
							state, 
							flags, 
							prio, 
							num_vmas, 
							total_vm);
                current_process++;
        } else {
        	current_process = -1;
        	num_processes = 0;

        	strcpy(buf, "");

        	kfree(processes);
        	processes = NULL;
        }
}

/*
 * Function called when accessing module
 */
static ssize_t kquery_call(struct file *file, const char __user *buf,
	size_t count, loff_t *ppos)
{
	int rc;
	char callbuf[MAX_CALL];

	if (count >= MAX_CALL)
		return -EINVAL;

	preempt_disable();
	
	respbuf = kmalloc(MAX_RESP, GFP_ATOMIC);
	if (respbuf == NULL) {
		preempt_enable(); 
		return -ENOSPC;
	}
	
	strcpy(respbuf,"");

	rc = copy_from_user(callbuf, buf, count);
	callbuf[MAX_CALL - 1] = '\0';

	if (strcmp(callbuf, "process_get_row") == 0)
		process_get_row(respbuf);

	preempt_enable();

	*ppos = 0;

	return count;  
}

/*
 * Return from a call to the module
 */
static ssize_t kquery_return(struct file *file, char __user *userbuf,
	size_t count, loff_t *ppos)
{
	int rc;

	preempt_disable();

	rc = strlen(respbuf) + 1;

	if (count < rc) {
		respbuf[count - 1] = '\0';
		rc = copy_to_user(userbuf, respbuf, count);
	} else {
		rc = copy_to_user(userbuf, respbuf, rc);
	}

	kfree(respbuf);
	respbuf = NULL;

	preempt_enable();

	*ppos = 0;

	return rc;
} 

/*
 * Override read and write
 */
static const struct file_operations myfops = {
	.read = kquery_return,
	.write = kquery_call,
};

struct dentry *dir, *file;

/*
 * Creates shared file
 */
static int __init kquery_mod_init(void)
{
	int file_value;

	dir = debugfs_create_dir(dir_name, NULL);
	if (dir == NULL) {
		printk(KERN_DEBUG 
			"kquery: error creating %s directory\n", dir_name);
		return -ENODEV;
	}

	file = debugfs_create_file(file_name, 0666, dir, &file_value, &myfops);
	if (file == NULL) {
		printk(KERN_DEBUG 
			"kquery: error creating %s file\n", file_name);
		return -ENODEV;
	}

	printk(KERN_DEBUG 
		"kquery: created new debugfs directory and file\n");

	return 0;
}

/*
 * Deletes shared file and clears memory
 */
static void __exit kquery_mod_exit(void)
{
	debugfs_remove(file);
	debugfs_remove(dir);
	if (respbuf != NULL)
		kfree(respbuf);
}

module_init(kquery_mod_init);
module_exit(kquery_mod_exit);
MODULE_LICENSE("GPL");