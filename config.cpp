#include "config.h"
#include "common.h"
#include <stdio.h>

#define CONFLINEBUFSZ 100

config_t g_conf = {false,0,0,0,0};

void initConfig(void){
	char buf[CONFLINEBUFSZ];
	FILE *fp = fopen("./config.txt", "r");
	fgets(buf, CONFLINEBUFSZ, fp);
	sscanf(buf, "%*s %f", &g_conf.DONUTXRATIO);
	fgets(buf, CONFLINEBUFSZ, fp);
	sscanf(buf, "%*s %f", &g_conf.DONUTYRATIO);
	fgets(buf, CONFLINEBUFSZ, fp);
	sscanf(buf, "%*s %f", &g_conf.DONUTINNERRATIO);
	fgets(buf, CONFLINEBUFSZ, fp);
	sscanf(buf, "%*s %f", &g_conf.DONUTOUTERRATIO);
	fclose(fp);
	
	g_conf.ready = true;
}

void writeConfig(void){
	FILE *fp = fopen("./config.txt", "w");
	fprintf(fp, "DONUTXRATIO %f\n", g_conf.DONUTXRATIO);
	fprintf(fp, "DONUTYRATIO %f\n", g_conf.DONUTXRATIO);
	fprintf(fp, "DONUTINNERRATIO %f\n", g_conf.DONUTXRATIO);
	fprintf(fp, "DONUTOUTERRATIO %f\n", g_conf.DONUTXRATIO);
	fclose(fp);
}