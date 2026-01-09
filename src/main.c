#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_PROCS 4096

typedef struct {
  int pid;
  char name[256];
  long rss_kb;
  unsigned long long proc_ticks;
  unsigned long long prev_proc_ticks;
  double cpu_percent;
} ProcInfo;

int cmp_cpu_desc(const void *a, const void *b) {
  const ProcInfo *p1 = a;
  const ProcInfo *p2 = b;
  if (p2->cpu_percent > p1->cpu_percent)
    return 1;
  if (p2->cpu_percent < p1->cpu_percent)
    return -1;
  return 0;
}

// total CPU
int read_total_cpu_ticks(unsigned long long *total) {
  FILE *f = fopen("/proc/stat", "r");
  if (!f)
    return -1;
  char line[512];
  if (!fgets(line, sizeof(line), f)) {
    fclose(f);
    return -1;
  }
  fclose(f);
  unsigned long long user, nice, system, idle, iowait, irq, softirq, steal,
      guest, guest_nice;
  int n = sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                 &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal,
                 &guest, &guest_nice);
  if (n < 4)
    return -1;
  unsigned long long sum = 0;

  if (n >= 1)
    sum += user;
  if (n >= 2)
    sum += nice;
  if (n >= 3)
    sum += system;
  if (n >= 4)
    sum += idle;
  if (n >= 5)
    sum += iowait;
  if (n >= 6)
    sum += irq;
  if (n >= 7)
    sum += softirq;
  if (n >= 8)
    sum += steal;
  if (n >= 9)
    sum += guest;
  if (n >= 10)
    sum += guest_nice;
  *total = sum;
  return 0;
}

// read_proc_tick
int read_proc_ticks(const char *pid, unsigned long long *ticks) {
  char path[256];
  snprintf(path, sizeof(path), "/proc/%s/stat", pid);
  FILE *f = fopen(path, "r");
  if (!f)
    return -1;
  char buff[4096];
  if (!fgets(buff, sizeof(buff), f)) {
    fclose(f);
    return -1;
  }
  fclose(f);

  char *ren = strrchr(buff, ')');
  if (!ren)
    return -1;
  char *p = ren + 2;
  char state;
  unsigned long long utime = 0;
  unsigned long long stime = 0;
  int ok = sscanf(p, "%c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu",
                  &state, &utime, &stime);
  if (ok != 3)
    return -1;
  *ticks = utime + stime;
  return 0;
}

// previous pid
typedef struct {
  int pid;
  unsigned long long prev_ticks;
} PrevEntry;

// find previous ticks
int find_prev(PrevEntry *prev, int prev_count, int pid) {
  for (int i = 0; i < prev_count; i++) {
    if (prev[i].pid == pid)
      return i;
  }
  return -1;
}

// check is_number
int is_number(const char *s) {
  if (!s || !*s)
    return 0;
  for (; *s != '\0'; s++) {
    if (!isdigit(*s))
      return 0;
  }
  return 1;
}

// read PID and Process Name
int read_pid(const char *pid, char *name, size_t size) {
  char path[256];
  snprintf(path, sizeof(path), "/proc/%s/stat", pid);

  FILE *f = fopen(path, "r");
  if (!f)
    return -1;
  char buff[4096];
  if (!fgets(buff, sizeof(buff), f)) {
    fclose(f);
    return -1;
  }
  fclose(f);
  char *l = strchr(buff, '(');
  char *r = strrchr(buff, ')');

  if (!l || !r || r <= l)
    return -1;

  size_t len = r - l - 1;
  if (len >= size)
    len = size - 1;
  strncpy(name, l + 1, len);
  name[len] = '\0';
  return 0;
}

// RSS
int read_rss_kb(const char *pid, long *rss_kb) {
  char path[256];
  snprintf(path, sizeof(path), "/proc/%s/status", pid);
  FILE *f = fopen(path, "r");
  if (!f)
    return -1;
  char line[512];
  while (fgets(line, sizeof(line), f)) {
    if (strstr(line, "VmRSS:") == line) {
      if (sscanf(line, "VmRSS: %ld kB", rss_kb) == 1) {
        fclose(f);
        return 0;
      }
    }
  }
  fclose(f);
  return -1;
}

int main() {

  printf("\033[?25l");
  PrevEntry prev[MAX_PROCS];
  int prev_count = 0;
  unsigned long long prev_total = 0;
  read_total_cpu_ticks(&prev_total);

  while (1) {
    unsigned long long total_now = 0;
    if (read_total_cpu_ticks(&total_now) != 0)
      total_now = prev_total;
    unsigned long long delta_total =
        (total_now >= prev_total) ? (total_now - prev_total) : 0;

    printf("\033[H\033[2J");
    printf("Minitop (Ctrl + C to quit)\n");
    printf("--------------------------\n");

    // Uptime and idle
    FILE *f = fopen("/proc/uptime", "r");
    if (f) {
      double uptime = 0.0;
      double idle_total = 0.0;
      if (fscanf(f, "%lf %lf", &uptime, &idle_total) == 2) {
        printf("Uptime: %.1f seconds | Idle: %.1f seconds\n", uptime,
               idle_total);
      }
    } else {
      perror("fopen");
      fclose(f);
      return 1;
    }

    // Load avg
    f = fopen("/proc/loadavg", "r");
    if (f) {
      double load1 = 0.0;
      double load2 = 0.0;
      double load3 = 0.0;
      if (fscanf(f, "%lf %lf %lf", &load1, &load2, &load3) == 3) {
        printf("Load average: %.2f %.2f %.2f\n", load1, load2, load3);
      }
      fclose(f);
    }

    // Process counts
    int proc_count = 0;
    DIR *dir = opendir("/proc");
    if (dir) {
      struct dirent *ent;
      while ((ent = readdir(dir)) != NULL) {
        if (is_number(ent->d_name)) {
          proc_count++;
        }
      }
      closedir(dir);
    }

    printf("Total processes: %d\n", proc_count);

    // PID - NAME
    printf("\nPID      CPU%%      RSS(KB)      NAME\n");
    ProcInfo procs[MAX_PROCS];
    int count = 0;

    dir = opendir("/proc");
    if (dir) {
      struct dirent *ent;
      while ((ent = readdir(dir)) != NULL && count < MAX_PROCS) {
        if (!is_number(ent->d_name))
          continue;
        int pid = atoi(ent->d_name);
        ProcInfo pi;
        memset(&pi, 0, sizeof(pi));
        pi.pid = pid;

        if (read_pid(ent->d_name, pi.name, sizeof(pi.name)) != 0)
          continue;
        if (read_rss_kb(ent->d_name, &pi.rss_kb) != 0)
          pi.rss_kb = 0;
        if (read_proc_ticks(ent->d_name, &pi.proc_ticks) != 0)
          continue;

        int idx = find_prev(prev, prev_count, pid);
        if (idx >= 0) {
          pi.prev_proc_ticks = prev[idx].prev_ticks;
        } else {
          pi.prev_proc_ticks = pi.proc_ticks;
        }
        unsigned long long delta_proc =
            (pi.proc_ticks >= pi.prev_proc_ticks)
                ? (pi.proc_ticks - pi.prev_proc_ticks)
                : 0;
        if (delta_proc > 0) {
          pi.cpu_percent = 100.0 * (double)delta_proc / (double)delta_total;
        } else {
          pi.cpu_percent = 0.0;
        }
        procs[count++] = pi;
      }
      closedir(dir);
    }

    prev_count = 0;
    for (int i = 0; i < count && prev_count < MAX_PROCS; i++) {
      prev[prev_count].pid = procs[i].pid;
      prev[prev_count].prev_ticks = procs[i].proc_ticks;
      prev_count++;
    }
    prev_total = total_now;

    qsort(procs, count, sizeof(ProcInfo), cmp_cpu_desc);
    int shown = (count < 15) ? count : 15;
    for (int i = 0; i < shown; i++) {

      printf("%-7d %5.1f    %8ld       %s\n", procs[i].pid,
             procs[i].cpu_percent, procs[i].rss_kb, procs[i].name);
    }
    fflush(stdout);
    sleep(1);
  };

  printf("\033[?25h");
  return 0;
}
