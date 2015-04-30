#include "loadstats.h"
#include "common.h"
#include <stdio.h>

#define LINEBUFSZ 100

cpustat_t cputot_stats = {0,0,0};
cpustat_t cpu1_stats = {0,0,0};
cpustat_t cpu2_stats = {0,0,0};
cpustat_t cpu3_stats = {0,0,0};
cpustat_t cpu4_stats = {0,0,0};


void updateStats(void){
	long double a[4], b[4], loadavg, busy, total;
	char buf[LINEBUFSZ];
	FILE *fp;

	fp = fopen("/proc/stat","r");
	fgets(buf, LINEBUFSZ, fp);
	sscanf(buf,"%*s %Lf %Lf %Lf %Lf",&a[0],&a[1],&a[2],&a[3]);
	busy = (a[0]+a[1]+a[2]);
	total = (busy+a[3]);
	cputot_stats.load = (int)(100*(busy-cputot_stats.busy)/(total-cputot_stats.total));
	cputot_stats.busy = busy;
	cputot_stats.total = total;	
	fgets(buf, LINEBUFSZ, fp);
	sscanf(buf,"%*s %Lf %Lf %Lf %Lf",&a[0],&a[1],&a[2],&a[3]);
	busy = (a[0]+a[1]+a[2]);
	total = (busy+a[3]);
	cpu1_stats.load = (int)(100*(busy-cpu1_stats.busy)/(total-cpu1_stats.total));
	cpu1_stats.busy = busy;
	cpu1_stats.total = total;
	fgets(buf, LINEBUFSZ, fp);
	sscanf(buf,"%*s %Lf %Lf %Lf %Lf",&a[0],&a[1],&a[2],&a[3]);
	busy = (a[0]+a[1]+a[2]);
	total = (busy+a[3]);
	cpu2_stats.load = (int)(100*(busy-cpu2_stats.busy)/(total-cpu2_stats.total));
	cpu2_stats.busy = busy;
	cpu2_stats.total = total;
	fgets(buf, LINEBUFSZ, fp);
	sscanf(buf,"%*s %Lf %Lf %Lf %Lf",&a[0],&a[1],&a[2],&a[3]);
	busy = (a[0]+a[1]+a[2]);
	total = (busy+a[3]);
	cpu3_stats.load = (int)(100*(busy-cpu3_stats.busy)/(total-cpu3_stats.total));
	cpu3_stats.busy = busy;
	cpu3_stats.total = total;
	fgets(buf, LINEBUFSZ, fp);
	sscanf(buf,"%*s %Lf %Lf %Lf %Lf",&a[0],&a[1],&a[2],&a[3]);
	busy = (a[0]+a[1]+a[2]);
	total = (busy+a[3]);
	cpu4_stats.load = (int)(100*(busy-cpu4_stats.busy)/(total-cpu4_stats.total));
	cpu4_stats.busy = busy;
	cpu4_stats.total = total;
	fclose(fp);

    return;	
}