// SPDX-License-Identifier: GPL-2.0
/*
 * procinfo_main.c - module entry/exit and the /proc/procinfo read interface
 * Owner: M1 (Aadya Shankar Jha, 590025709) - team lead
 *
 * When a user program calls read() on /proc/procinfo, the CPU switches from
 * user mode to kernel mode (system call). The VFS calls pi_open() and then
 * seq_read(), which calls pi_show() below. pi_show() runs in kernel mode,
 * takes a snapshot of the task list (procinfo_tasks.c), formats it as text
 * into a kernel buffer, and seq_read() copies that text back to the user
 * buffer with copy_to_user() before returning to user mode.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include "procinfo.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group G12 (B10), CSEG2060 Operating Systems, UPES");
MODULE_DESCRIPTION("Process Information Driver: lists task_struct data via /proc");
MODULE_VERSION(PI_VERSION);

/* Load-time parameter: insmod procinfo.ko show_kthreads=0 */
bool show_kthreads = true;
module_param(show_kthreads, bool, 0444);
MODULE_PARM_DESC(show_kthreads, "Include kernel threads in the default view (default: 1)");

static struct proc_dir_entry *pi_entry;

/* Called (in kernel mode) every time /proc/procinfo is read. */
static int pi_show(struct seq_file *m, void *v)
{
	struct pi_filter f;
	struct pi_snapshot s;
	int i, shown = 0, ret;

	/* Copy the current filter so the lock is held only briefly. */
	mutex_lock(&pi_ctl_lock);
	f = pi_cur_filter;
	mutex_unlock(&pi_ctl_lock);

	if (f.mode == PI_MODE_TREE)
		f.threads = false;	/* the tree is a tree of processes */

	ret = pi_snapshot_take(&s, &f);
	if (ret)
		return ret;

	if (f.mode == PI_MODE_LIST) {
		for (i = 0; i < s.count; i++)
			if (pi_task_match(&s.tasks[i], &f))
				shown++;
	}

	seq_printf(m, "# procinfo v%s - task_struct snapshot taken in kernel mode\n",
		   PI_VERSION);
	seq_printf(m, "# tasks=%d stored=%d shown=%d running=%d sleeping=%d disk_sleep=%d stopped=%d zombie=%d idle=%d other=%d snapshot_ns=%llu\n",
		   s.total, s.count, f.mode == PI_MODE_LIST ? shown : -1,
		   s.state_count[PI_ST_R], s.state_count[PI_ST_S],
		   s.state_count[PI_ST_D], s.state_count[PI_ST_T],
		   s.state_count[PI_ST_Z], s.state_count[PI_ST_I],
		   s.state_count[PI_ST_OTHER], s.elapsed_ns);
	pi_filter_describe(m, &f);
	pi_print_header(m);

	if (f.mode == PI_MODE_TREE) {
		ret = pi_print_tree(m, &s, &f);
	} else {
		for (i = 0; i < s.count; i++)
			if (pi_task_match(&s.tasks[i], &f))
				pi_print_task(m, &s.tasks[i], 0);
	}

	mutex_lock(&pi_ctl_lock);
	pi_stats.reads++;
	pi_stats.last_ns = s.elapsed_ns;
	pi_stats.last_tasks = s.count;
	pi_stats.last_bytes = m->count;
	mutex_unlock(&pi_ctl_lock);

	pi_snapshot_free(&s);
	return ret;
}

static int pi_open(struct inode *inode, struct file *file)
{
	/* Size the seq_file buffer from the current number of tasks so the
	 * output normally fits in one pass (about 120 bytes per row). */
	size_t size = (size_t)(pi_count_tasks(true) + 64) * 128 + 4096;

	return single_open_size(file, pi_show, NULL, size);
}

static const struct proc_ops pi_proc_ops = {
	.proc_open    = pi_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

static int __init pi_init(void)
{
	int ret;

	pi_filter_reset(&pi_cur_filter);

	pi_entry = proc_create(PI_PROC_NAME, 0444, NULL, &pi_proc_ops);
	if (!pi_entry) {
		pr_err("procinfo: cannot create /proc/%s\n", PI_PROC_NAME);
		return -ENOMEM;
	}

	ret = pi_ctl_init();
	if (ret) {
		proc_remove(pi_entry);
		return ret;
	}

	pr_info("procinfo: loaded v%s, created /proc/%s and /proc/%s\n",
		PI_VERSION, PI_PROC_NAME, PI_CTL_NAME);
	pr_info("procinfo: init runs in kernel mode on behalf of PID %d (%s); %d processes visible, show_kthreads=%d\n",
		task_pid_nr(current), current->comm, pi_count_tasks(false),
		show_kthreads);
	return 0;
}

static void __exit pi_exit(void)
{
	pi_ctl_exit();
	proc_remove(pi_entry);
	pr_info("procinfo: unloaded (reads=%llu, ctl writes=%llu)\n",
		pi_stats.reads, pi_stats.writes);
}

module_init(pi_init);
module_exit(pi_exit);
