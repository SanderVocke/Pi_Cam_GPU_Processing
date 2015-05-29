#ifndef _CONFIG_H_
#define _CONFIG_H_

typedef struct config_t{
	bool ready;
	float DONUTXRATIO;
	float DONUTYRATIO;
	float DONUTINNERRATIO;
	float DONUTOUTERRATIO;
	int CAPTURE_WIDTH;
	int CAPTURE_HEIGHT;
	int CAPTURE_FPS;
	int LOWRES_WIDTH;
	float REDPARAMS[4];
	float BLUEPARAMS[4];
	int EXPOSURE;
	int AWB;
	int METERING;
	int FX;
}config_t;

extern config_t g_conf;

void initConfig(void);
void writeConfig(void);

#endif