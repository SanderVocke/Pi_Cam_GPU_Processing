#include "config.h"
#include "common.h"
#include <stdio.h>

#define CONFLINEBUFSZ 100

config_t g_conf = {false,0,0,0,0,0,0,0};

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
	fgets(buf, CONFLINEBUFSZ, fp);
	sscanf(buf, "%*s %d", &g_conf.CAPTURE_WIDTH);
	fgets(buf, CONFLINEBUFSZ, fp);
	sscanf(buf, "%*s %d", &g_conf.CAPTURE_HEIGHT);
	fgets(buf, CONFLINEBUFSZ, fp);
	sscanf(buf, "%*s %d", &g_conf.CAPTURE_FPS);
	fgets(buf, CONFLINEBUFSZ, fp);
	sscanf(buf, "%*s %d", &g_conf.LOWRES_WIDTH);
	fgets(buf, CONFLINEBUFSZ, fp);
	sscanf(buf, "%*s %f %f %f %f", &g_conf.REDPARAMS[0], &g_conf.REDPARAMS[1], &g_conf.REDPARAMS[2], &g_conf.REDPARAMS[3]);
	fgets(buf, CONFLINEBUFSZ, fp);
	sscanf(buf, "%*s %f %f %f %f", &g_conf.BLUEPARAMS[0], &g_conf.BLUEPARAMS[1], &g_conf.BLUEPARAMS[2], &g_conf.BLUEPARAMS[3]);
	fgets(buf, CONFLINEBUFSZ, fp);
	sscanf(buf, "%*s %f %f %f %f", &g_conf.REDPARAMSLOW[0], &g_conf.REDPARAMSLOW[1], &g_conf.REDPARAMSLOW[2], &g_conf.REDPARAMSLOW[3]);
	fgets(buf, CONFLINEBUFSZ, fp);
	sscanf(buf, "%*s %f %f %f %f", &g_conf.BLUEPARAMSLOW[0], &g_conf.BLUEPARAMSLOW[1], &g_conf.BLUEPARAMSLOW[2], &g_conf.BLUEPARAMSLOW[3]);
	fgets(buf, CONFLINEBUFSZ, fp);
	sscanf(buf, "%*s %d", &g_conf.EXPOSURE);
	fgets(buf, CONFLINEBUFSZ, fp);
	sscanf(buf, "%*s %d", &g_conf.AWB);
	fgets(buf, CONFLINEBUFSZ, fp);
	sscanf(buf, "%*s %d", &g_conf.METERING);
	fgets(buf, CONFLINEBUFSZ, fp);
	sscanf(buf, "%*s %d", &g_conf.FX);
	fclose(fp);	
	g_conf.ready = true;
}

void writeConfig(void){
	FILE *fp = fopen("./config.txt", "w");
	fprintf(fp, "DONUTXRATIO %f\n", g_conf.DONUTXRATIO);
	fprintf(fp, "DONUTYRATIO %f\n", g_conf.DONUTXRATIO);
	fprintf(fp, "DONUTINNERRATIO %f\n", g_conf.DONUTXRATIO);
	fprintf(fp, "DONUTOUTERRATIO %f\n", g_conf.DONUTXRATIO);
	fprintf(fp, "CAPTURE_WIDTH %d\n", g_conf.CAPTURE_WIDTH);
	fprintf(fp, "CAPTURE_HEIGHT %d\n", g_conf.CAPTURE_HEIGHT);
	fprintf(fp, "CAPTURE_FPS %d\n", g_conf.CAPTURE_FPS);
	fprintf(fp, "LOWRES_WIDTH %d\n", g_conf.LOWRES_WIDTH);
	fprintf(fp, "RED_PARAMS %f %f %f %f\n", g_conf.REDPARAMS[0], g_conf.REDPARAMS[1], g_conf.REDPARAMS[2], g_conf.REDPARAMS[3]);
	fprintf(fp, "BLUE_PARAMS %f %f %f %f\n", g_conf.BLUEPARAMS[0], g_conf.BLUEPARAMS[1], g_conf.BLUEPARAMS[2], g_conf.BLUEPARAMS[3]);
	fprintf(fp, "RED_PARAMS %f %f %f %f\n", g_conf.REDPARAMSLOW[0], g_conf.REDPARAMSLOW[1], g_conf.REDPARAMSLOW[2], g_conf.REDPARAMSLOW[3]);
	fprintf(fp, "BLUE_PARAMS %f %f %f %f\n", g_conf.BLUEPARAMSLOW[0], g_conf.BLUEPARAMSLOW[1], g_conf.BLUEPARAMSLOW[2], g_conf.BLUEPARAMSLOW[3]);
	fprintf(fp, "EXPOSURE %d\n", g_conf.EXPOSURE);
	fprintf(fp, "AWB %d\n", g_conf.AWB);
	fprintf(fp, "METERING %d\n", g_conf.METERING);
	fprintf(fp, "FX %d\n", g_conf.FX);
	fclose(fp);
}