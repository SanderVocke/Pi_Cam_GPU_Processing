#ifndef __LOADSTATS_H__
#define __LOADSTATS_H__

typedef struct cpustat_t{
	long double busy;
	long double total;
	int load;
}cpustat_t;

extern cpustat_t cputot_stats;
extern cpustat_t cpu1_stats;
extern cpustat_t cpu2_stats;
extern cpustat_t cpu3_stats;
extern cpustat_t cpu4_stats;

void updateStats(void);

#endif