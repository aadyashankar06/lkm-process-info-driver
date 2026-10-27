/* SPDX-License-Identifier: GPL-2.0 */
/*
 * procinfo.h - shared definitions for the Process Information Driver
 *
 * Project : Loadable Kernel Module: Process Information Driver (P12, G12)
 * Course  : CSEG2060 Operating Systems, B.Tech CSE (AI & ML), Batch B10
 *
 * The module is built from three source files:
 *   procinfo_main.c  - module init/exit, /proc/procinfo, output   (M1)
 *   procinfo_tasks.c - task_struct snapshot, states, tree view    (M2)
 *   procinfo_ctl.c   - /proc/procinfo_ctl user->kernel channel     (M3)
 */
#ifndef PROCINFO_H
#define PROCINFO_H

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#define PI_VERSION         "1.0"
#define PI_PROC_NAME       "procinfo"
#define PI_CTL_NAME        "procinfo_ctl"
#define PI_COMM_FILTER_LEN 16
#define PI_CTL_MAX_WRITE   128
#define PI_MAX_TREE_INDENT 24

enum pi_mode { PI_MODE_LIST = 0, PI_MODE_TREE = 1 };

/* What user space asked to see (set by writing to /proc/procinfo_ctl). */
struct pi_filter {
	pid_t pid;                       /* -1 = any                     */
	pid_t ppid;                      /* -1 = any                     */
	char  state;                     /* 0  = any, else R,S,D,T,Z,I.. */
	char  comm[PI_COMM_FILTER_LEN];  /* "" = any, else substring     */
	bool  threads;                   /* list every thread            */
	bool  kthreads;                  /* include kernel threads       */
	enum pi_mode mode;               /* list or tree                 */
};

/* One row of the snapshot: a copy of the task_struct fields we report. */
struct pi_task {
	pid_t pid;            /* thread-group id (what user space calls PID) */
	pid_t tid;            /* task id (equal to pid for the main thread)  */
	pid_t ppid;           /* tgid of real_parent                         */
	char  state;          /* task_state_to_char()                        */
	int   prio;           /* p->prio - MAX_RT_PRIO (same as /proc stat)  */
	int   nice;           /* task_nice()                                 */
	int   nr_threads;     /* threads in the thread group                 */
	unsigned long rss_kb; /* resident memory, kB (0 for kernel threads)  */
	uid_t uid;            /* real uid                                    */
	u64   cpu_ms;         /* utime + stime of this task, ms              */
	u64   start_s;        /* start time, seconds since boot              */
	bool  kthread;        /* PF_KTHREAD                                  */
	char  comm[TASK_COMM_LEN];
};

/* Index into pi_snapshot.state_count[] */
enum { PI_ST_R, PI_ST_S, PI_ST_D, PI_ST_T, PI_ST_Z, PI_ST_I, PI_ST_OTHER, PI_ST_MAX };

struct pi_snapshot {
	struct pi_task *tasks;
	int  count;                  /* rows stored                         */
	int  capacity;
	int  total;                  /* tasks seen while walking the list   */
	int  state_count[PI_ST_MAX];
	u64  elapsed_ns;             /* time spent inside rcu_read_lock()   */
};

struct pi_stats {
	u64    reads;
	u64    writes;
	u64    rejected;
	u64    last_ns;
	int    last_tasks;
	size_t last_bytes;
};

/* ---- procinfo_main.c (M1) ---- */
extern bool show_kthreads;

/* ---- procinfo_ctl.c (M3) ---- */
extern struct mutex     pi_ctl_lock;
extern struct pi_filter pi_cur_filter;
extern struct pi_stats  pi_stats;
int  pi_ctl_init(void);
void pi_ctl_exit(void);
void pi_filter_reset(struct pi_filter *f);
void pi_filter_describe(struct seq_file *m, const struct pi_filter *f);

/* ---- procinfo_tasks.c (M2) ---- */
int  pi_count_tasks(bool threads);
int  pi_snapshot_take(struct pi_snapshot *s, const struct pi_filter *f);
void pi_snapshot_free(struct pi_snapshot *s);
const char *pi_state_name(char st);
bool pi_task_match(const struct pi_task *t, const struct pi_filter *f);
void pi_print_header(struct seq_file *m);
void pi_print_task(struct seq_file *m, const struct pi_task *t, int depth);
int  pi_print_tree(struct seq_file *m, struct pi_snapshot *s, const struct pi_filter *f);

#endif /* PROCINFO_H */
