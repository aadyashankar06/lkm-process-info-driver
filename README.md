# Loadable Kernel Module: Process Information Driver

**CSEG2060 Operating Systems | Project P12 | Group G12 | Batch B10 | UPES Dehradun**

A Linux loadable kernel module (LKM) that walks the kernel's process list
(`struct task_struct`, the Linux Process Control Block) and publishes it
through `/proc/procinfo`, with a second file `/proc/procinfo_ctl` that lets a
user program send commands *into* the kernel. Together they show the full
user-mode <-> kernel-mode round trip.

| Member | SAP ID | Module owned |
|---|---|---|
| Aadya Shankar Jha (lead) | 590025709 | `kernel/procinfo_main.c`, `kernel/procinfo.h`, Makefiles, README |
| Manvi Sharma | 590028237 | `kernel/procinfo_tasks.c` (task_struct snapshot, states, tree) |
| Piyush Mani Tripathi | 590021763 | `kernel/procinfo_ctl.c`, `user/procinfo_cli.c` (user <-> kernel channel) |
| Sarthak Bansal | 590024203 | `user/proc_walk.c`, `scripts/*.sh` (testing, demo, benchmark) |
| Vibha Dubey | 590026089 | `analysis/validate.py`, `analysis/plot_results.py`, `results/` |

## Features

- Lists every process (or every thread) with **PID, TID, PPID, state, priority,
  nice, thread count, RSS memory, UID, CPU time, start time and name**.
- **Process tree** view (parent -> children), or the subtree of one PID.
- **Kernel-side filtering** by PID, PPID, state, or name, set from user space by
  writing to `/proc/procinfo_ctl` (validated, all-or-nothing updates).
- **Consistent snapshot**: the task list is copied under `rcu_read_lock()` in one
  short pass; formatting happens afterwards, outside the lock. The time spent
  in the lock is reported (`snapshot_ns`).
- Load-time module parameter `show_kthreads=0|1`.
- Test programs that create zombie, stopped and multi-threaded processes.
- Benchmark against the classic method (`/proc/<pid>/stat` walk and `ps`) and
  validation of every field against `ps`.

## Repository layout

```
kernel/    procinfo.h  procinfo_main.c  procinfo_tasks.c  procinfo_ctl.c  Makefile
user/      procinfo_cli.c (client)  proc_walk.c (baseline)  Makefile
scripts/   load.sh unload.sh demo.sh make_states.sh strace_demo.sh bench.sh run_all_experiments.sh
analysis/  validate.py  plot_results.py
results/   benchmark CSV, validation, sample outputs, figures
```

## Requirements

Ubuntu 22.04 / 24.04 (tested in a VM, never on the host OS):

```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r) git strace python3-matplotlib procps
```

## Build, load, use, unload

```bash
make                                  # builds kernel/procinfo.ko and user/ tools
sudo insmod kernel/procinfo.ko        # or: bash scripts/load.sh  (show_kthreads=0 optional)
sudo dmesg | tail -n 2                # module messages

cat /proc/procinfo | head             # kernel -> user: all tasks
./user/procinfo_cli -s Z              # only zombies
./user/procinfo_cli -t -K             # process tree, user processes only
./user/procinfo_cli -T -c bash        # every thread of processes named *bash*
./user/procinfo_cli -v -p $$          # explain every user/kernel mode switch
./user/procinfo_cli -S                # control channel status

echo "ppid=1 state=S" > /proc/procinfo_ctl   # user -> kernel, raw
cat /proc/procinfo_ctl
echo reset > /proc/procinfo_ctl

sudo rmmod procinfo                   # or: bash scripts/unload.sh
```

### Control commands (`/proc/procinfo_ctl`)

`reset` | `list` | `tree` | `pid=N|any` | `ppid=N|any` |
`state=R|S|D|T|Z|I|any` | `comm=TEXT|any` | `threads=0|1` | `kthreads=0|1`

Invalid input is rejected with `EINVAL` and the previous filter is kept.

### Output columns

`PID TID PPID ST STATE PRI NI THR RSS_KB UID CPU_MS START_S COMMAND`
(kernel threads are shown in `[brackets]`, like `ps`).

## Experiments

```bash
bash scripts/demo.sh                 # live demo, step by step (used in the video)
bash scripts/run_all_experiments.sh     # every result + figure used in the report
```

`bench.sh` compares, for 0 to 2000 extra processes:
1. our module (`/proc/procinfo`, one open + a few reads),
2. the classic way (`proc_walk`: open/read/close `/proc/<pid>/stat` per process),
3. `ps -eo pid,ppid,stat,comm`.

## How user mode talks to kernel mode here

```
 user mode (EL0/ring 3)                 kernel mode (EL1/ring 0)
 ----------------------                 ------------------------
 procinfo_cli: write(fd,"pid=1") --syscall--> VFS -> pi_ctl_write()
                                              copy_from_user(), validate, mutex
 procinfo_cli: read(fd, buf)     --syscall--> VFS -> seq_read() -> pi_show()
                                              rcu_read_lock(); for_each_process()
                                              copy task_struct fields; unlock
                 <--copy_to_user()----------- format text into seq_file buffer
```

## License

GPL-2.0 (required for a kernel module that uses GPL-only kernel symbols).
