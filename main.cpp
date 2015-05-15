#include "loadstats.h"
#include "graphics.h"
#include "camera.h"
#include "common.h"
#include "config.h"
#include "window.h"
#include <stdio.h>
#include <curses.h>
#include <time.h>
#include <unistd.h>
#include <cmath>
#include "i2c_motor.h"

#include "GLES2/gl2.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

#define PI (3.141592653589793)

int dWidth, dHeight; //width and height of "dedonuted" image
bool gHaveI2C = false;
bool showWindow = false;

#define UPDATERATE 10
#define MAX_COORDS 100

#define NUMKEYS 4
const char* keys[NUMKEYS] = {
	"s: show snapshot window",
	"w: save framebuffers",
	"arrow keys: move bot",
	"q: quit"
};

void drawCurses(float fr, long nsec_curses, long nsec_readframe, long nsec_putframe, long nsec_draw, long nsec_getdata){
	updateStats();
	mvprintw(0,0,"Framerate: %g  ",fr);	
	mvprintw(2,0,"CPU Total: %d   ",cputot_stats.load);
	mvprintw(3,0,"CPU1: %d  ",cpu1_stats.load);
	mvprintw(4,0,"CPU2: %d  ",cpu2_stats.load);
	mvprintw(5,0,"CPU3: %d  ",cpu3_stats.load);
	mvprintw(6,0,"CPU4: %d  ",cpu4_stats.load);
	
	mvprintw(8,0,"Left Speed: %d     ",
		getDirection(LEFT_MOTOR)==FORWARD ? getSpeed(LEFT_MOTOR) : -1*((int)getSpeed(LEFT_MOTOR)));
	mvprintw(9,0,"Right Speed: %d     ",
		getDirection(RIGHT_MOTOR)==FORWARD ? getSpeed(RIGHT_MOTOR) : -1*((int)getSpeed(RIGHT_MOTOR)));
		
	if(gHaveI2C) mvprintw(11,0,"I2C ON  ");
	else mvprintw(11,0,"I2C FAIL");
	
	mvprintw(13,0,"msec Curses: %d   ",nsec_curses/1000000);
	mvprintw(14,0,"msec Readframe: %d   ",nsec_readframe/1000000);
	mvprintw(15,0,"msec Putframe: %d   ",nsec_putframe/1000000);
	mvprintw(16,0,"msec Draw: %d   ",nsec_draw/1000000);
	mvprintw(17,0,"msec Getdata: %d   ",nsec_getdata/1000000);
	
	mvprintw(0,30,"Controls:");
	int h=0;
	for(h=0; h<NUMKEYS; h++) mvprintw(h+3,30,keys[h]);
	refresh();
}

void initDeDonutTextures(GfxTexture *targetmap){
	//this initializes the DeDonut texture, which is a texture that maps pixels of dedonuted to the donuted texture space (basically coordinate translation).

	//approximate dedonuted width based on circumference
	float widthf = (PI*g_conf.CAPTURE_WIDTH); 
	//approximate dedonuted height based on difference of radii of donut
	float heightf = ((g_conf.DONUTOUTERRATIO-g_conf.DONUTINNERRATIO)*g_conf.CAPTURE_WIDTH); 
	int width = (int)widthf;
	//Now a dirty way to get rid of the texture size limitation
	if(width>2048) width = 2048;
	int height = (int)heightf;	
	dWidth = width;
	dHeight = height;
	DBG("Dedonut: %dx%d", width, height);
	targetmap->CreateRGBA(width,height);
	targetmap->GenerateFrameBuffer();
	
	//now, generate the pixels
	unsigned char* data = (unsigned char*)calloc(1,4*(size_t)width*(size_t)height);
	int i,j;
	float x,y; //pixel
	float cx,cy; //center
	float rin,rout; //radii
	float d; //distance
	float dx, dy; //cartesian distances
	float phi; //polar angle to top
	float xout,yout; //mapping in floats (normalized)
	
	//donut parameters
	cx = g_conf.DONUTXRATIO;
	cy = g_conf.DONUTYRATIO;
	rin = g_conf.DONUTINNERRATIO;
	rout = g_conf.DONUTOUTERRATIO;
	for(i=0; i<width; i++)
		for(j=0; j<height; j++){
			//normalize coordinates
			x = ((float)i)/(float)width;
			y = ((float)j)/(float)height;
			//calculations
			phi = x*2*PI;
			d = (1-y)*(rout-rin)+rin;
			dx = d*sin(phi);
			dy = d*cos(phi);
			xout = cx+dx;
			yout = cy+dy;
			
			//store in pixel form
			data[(width*j+i)*4] = (int)(xout*255.0);
			data[(width*j+i)*4+1] = (int)(yout*255.0);
			data[(width*j+i)*4+3] = 255;
		}
	targetmap->SetPixels(data);
	free(data);	
	
	return;
}

int main(int argc, const char **argv)
{
	updateStats(); //baseline
	initConfig(); //get program settings
	if(startI2CMotor()) gHaveI2C = true;

	//init graphics and camera
	DBG("Start.");
	DBG("Initializing graphics and camera.");
	CCamera* cam = StartCamera(g_conf.CAPTURE_WIDTH, g_conf.CAPTURE_HEIGHT, g_conf.CAPTURE_FPS, 1, false); //camera
	InitGraphics();
	DBG("Camera resolution: %dx%d", g_conf.CAPTURE_WIDTH, g_conf.CAPTURE_HEIGHT);
	
	//set camera settings
	raspicamcontrol_set_metering_mode(cam->CameraComponent, METERINGMODE_AVERAGE);
	
	GfxTexture ytexture, utexture, vtexture, rgbtexture, rgblowtexture, thresholdtexture, thresholdlowtexture;
	GfxTexture dedonutmap, horsumtexture1, horsumtexture2, versumtexture1, versumtexture2, coordtexture;		
	GfxTexture horsumlowtexture1, versumlowtexture1, horsumlowtexture2, versumlowtexture2;
	DBG("Creating Textures.");
	//DeDonut RGB textures
	DBG("Max texture size: %d", GL_MAX_TEXTURE_SIZE);
	initDeDonutTextures(&dedonutmap);	
	//create YUV textures
	ytexture.CreateGreyScale(g_conf.CAPTURE_WIDTH, g_conf.CAPTURE_HEIGHT, NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	utexture.CreateGreyScale(g_conf.CAPTURE_WIDTH/2, g_conf.CAPTURE_HEIGHT/2, NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	vtexture.CreateGreyScale(g_conf.CAPTURE_WIDTH/2, g_conf.CAPTURE_HEIGHT/2, NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	//Main combined RGB textures
	rgbtexture.CreateRGBA(dWidth,dHeight,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	rgbtexture.GenerateFrameBuffer();
	thresholdtexture.CreateRGBA(dWidth,dHeight,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	thresholdtexture.GenerateFrameBuffer();
	horsumtexture1.CreateRGBA((dWidth/64)+1,dHeight,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	horsumtexture1.GenerateFrameBuffer();
	horsumtexture2.CreateRGBA(1,dHeight,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	horsumtexture2.GenerateFrameBuffer();
	versumtexture1.CreateRGBA(dWidth,(dHeight/64)+1,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	versumtexture1.GenerateFrameBuffer();
	versumtexture2.CreateRGBA(dWidth,1,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	versumtexture2.GenerateFrameBuffer();
	coordtexture.CreateRGBA(MAX_COORDS, 1,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	coordtexture.GenerateFrameBuffer();
	//showWindowd RGB textures
	float lowhf = ((float)dHeight/(float)dWidth);
	int lowh = (int)(lowhf * g_conf.LOWRES_WIDTH);
	if(!lowh) lowh = 1;
	rgblowtexture.CreateRGBA(g_conf.LOWRES_WIDTH, lowh,NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	rgblowtexture.GenerateFrameBuffer();
	thresholdlowtexture.CreateRGBA(g_conf.LOWRES_WIDTH, lowh,NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	thresholdlowtexture.GenerateFrameBuffer();
	horsumlowtexture1.CreateRGBA(200,lowh,NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	horsumlowtexture1.GenerateFrameBuffer();
	horsumlowtexture2.CreateRGBA(20,lowh,NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	horsumlowtexture2.GenerateFrameBuffer();
	versumlowtexture1.CreateRGBA(g_conf.LOWRES_WIDTH,100,NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	versumlowtexture1.GenerateFrameBuffer();
	versumlowtexture2.CreateRGBA(g_conf.LOWRES_WIDTH,20,NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	versumlowtexture2.GenerateFrameBuffer();
	
	
	//Start the processing loop.
	DBG("Starting process loop.");
	
	long int start_time;
	double total_frameset_time_s=0;
	long int time_difference;
	struct timespec gettime_now;
	clock_gettime(CLOCK_REALTIME, &gettime_now);
	start_time = gettime_now.tv_nsec ;
	double total_time_s = 0;
	bool do_pipeline = false;
	
	struct timespec t_start, t_curses, t_readframe, t_putframe, t_draw, t_getdata;
	long int nsec_curses, nsec_readframe, nsec_putframe, nsec_draw, nsec_getdata;

	initscr();      /* initialize the curses library */
	keypad(stdscr, TRUE);  /* enable keyboard mapping */
	nonl();         /* tell curses not to do NL->CR/NL on output */
	cbreak();       /* take input chars one at a time, no wait for \n */
	clear();
	nodelay(stdscr, TRUE);

	for(long i = 0;; i++)
	{
		clock_gettime(CLOCK_REALTIME, &t_start);
		
		if(showWindow){
			SDL_Rect inrect = {0, 0, g_conf.LOWRES_WIDTH, lowh};
			SDL_Rect thresholdrect = {0, lowh+200, g_conf.LOWRES_WIDTH, lowh};
			SDL_Rect versum1rect = {0,lowh+90,g_conf.LOWRES_WIDTH,100};
			SDL_Rect versum2rect = {0,lowh+50,g_conf.LOWRES_WIDTH,20};
			SDL_Rect horsum1rect = {g_conf.LOWRES_WIDTH,lowh+200,200,lowh};
			SDL_Rect horsum2rect = {g_conf.LOWRES_WIDTH+240,lowh+200,20,lowh};
			
			rgblowtexture.Show(&inrect);
			thresholdlowtexture.Show(&thresholdrect);
			if(showWindow){
				versumlowtexture1.Show(&versum1rect);
				versumlowtexture2.Show(&versum2rect);
				horsumlowtexture1.Show(&horsum1rect);
				horsumlowtexture2.Show(&horsum2rect);
			}
			
			showWindow = false;
		}
		
		int ch = getch();
		if(ch != ERR)
		{
			switch(ch){
			case KEY_LEFT:
				gHaveI2C = setSpeedDir(LEFT_MOTOR, BACKWARD, MAX_SPEED);
				gHaveI2C = setSpeedDir(RIGHT_MOTOR,FORWARD, MAX_SPEED);
				break;
			case KEY_UP:
				gHaveI2C = setSpeedDir(RIGHT_MOTOR,FORWARD, MAX_SPEED);
				gHaveI2C = setSpeedDir(LEFT_MOTOR, FORWARD, MAX_SPEED);
				break;
			case KEY_DOWN:
				gHaveI2C = setSpeedDir(RIGHT_MOTOR,BACKWARD, MAX_SPEED);
				gHaveI2C = setSpeedDir(LEFT_MOTOR, BACKWARD, MAX_SPEED);
				break;
			case KEY_RIGHT:
				gHaveI2C = setSpeedDir(RIGHT_MOTOR, BACKWARD, MAX_SPEED);
				gHaveI2C = setSpeedDir(LEFT_MOTOR,FORWARD, MAX_SPEED);
				break;
			case 's': //save framebuffers
				showWindow = true;
				break;
			case 'w': //save framebuffers
				//SaveFrameBuffer("tex_fb.png");
				rgbtexture.Save("./captures/tex_rgb.png");
				thresholdtexture.Save("./captures/tex_out.png");
				break;
			case 'q': //quit
				endwin();
				exit(1);
			}

			move(0,0);
			refresh();
		}
		else{ //no keypress
			gHaveI2C = setSpeed(RIGHT_MOTOR,0);
			gHaveI2C = setSpeed(LEFT_MOTOR, 0);
		}
		
		clock_gettime(CLOCK_REALTIME, &t_curses);
		
		//spin until we have a camera frame
		const void* frame_data; int frame_sz;
		while(!cam->BeginReadFrame(0,frame_data,frame_sz)) {usleep(500);};
		clock_gettime(CLOCK_REALTIME, &t_readframe);
		//lock the chosen frame buffer, and copy it directly into the corresponding open gl texture
		const uint8_t* data = (const uint8_t*)frame_data;
		int ypitch = g_conf.CAPTURE_WIDTH;
		int ysize = ypitch*g_conf.CAPTURE_HEIGHT;
		int uvpitch = g_conf.CAPTURE_WIDTH/2;
		int uvsize = uvpitch*g_conf.CAPTURE_HEIGHT/2;
		//int upos = ysize+16*uvpitch;
		//int vpos = upos+uvsize+4*uvpitch;
		int upos = ysize;
		int vpos = upos+uvsize;
		//printf("Frame data len: 0x%x, ypitch: 0x%x ysize: 0x%x, uvpitch: 0x%x, uvsize: 0x%x, u at 0x%x, v at 0x%x, total 0x%x\n", frame_sz, ypitch, ysize, uvpitch, uvsize, upos, vpos, vpos+uvsize);		
		ytexture.SetPixels(data);
		utexture.SetPixels(data+upos);
		vtexture.SetPixels(data+vpos);
		cam->EndReadFrame(0);
		
		clock_gettime(CLOCK_REALTIME, &t_putframe);
		
		
		
		//begin frame
		BeginFrame();
			
		DrawYUVTextureRect(&ytexture,&utexture,&vtexture,&dedonutmap,-1.f,-1.f,1.f,1.f,&rgbtexture);
		
		//make thresholded
		DrawThresholdRect(&rgbtexture, -1.0f, -1.0f, 1.0f, 1.0f, &thresholdtexture); 
		
		//sum rows/columns
		DrawHorSum1(&thresholdtexture, -1.0f, -1.0f, 1.0f, 1.0f, &horsumtexture1);
		DrawHorSum2(&horsumtexture1, -1.0f, -1.0f, 1.0f, 1.0f, &horsumtexture2);
		DrawVerSum1(&thresholdtexture, -1.0f, -1.0f, 1.0f, 1.0f, &versumtexture1);
		DrawVerSum2(&versumtexture1, -1.0f, -1.0f, 1.0f, 1.0f, &versumtexture2);
		
		//showWindow for preview window on this frame
		if(showWindow){
			DrawTextureRect(&rgbtexture, -1.0f, -1.0f, 1.0f, 1.0f, &rgblowtexture);
			DrawTextureRect(&thresholdtexture, -1.0f, -1.0f, 1.0f, 1.0f, &thresholdlowtexture);
			DrawTextureRect(&horsumtexture1, -1.0f, -1.0f, 1.0f, 1.0f, &horsumlowtexture1);
			DrawTextureRect(&horsumtexture2, -1.0f, -1.0f, 1.0f, 1.0f, &horsumlowtexture2);
			DrawTextureRect(&versumtexture1, -1.0f, -1.0f, 1.0f, 1.0f, &versumlowtexture1);
			DrawTextureRect(&versumtexture2, -1.0f, -1.0f, 1.0f, 1.0f, &versumlowtexture2);
		}
		
		EndFrame();
		
		clock_gettime(CLOCK_REALTIME, &t_draw);
		
		//get summed data back to host (CPU side)
		versumtexture2.Get();
		horsumtexture2.Get();
		versumtexture1.Get();
		horsumtexture1.Get();
		
		clock_gettime(CLOCK_REALTIME, &t_getdata);
		
		//benchmarking
		nsec_curses = t_curses.tv_nsec - t_start.tv_nsec;
		nsec_readframe = t_readframe.tv_nsec - t_curses.tv_nsec;
		nsec_putframe = t_putframe.tv_nsec - t_readframe.tv_nsec;
		nsec_draw = t_draw.tv_nsec - t_putframe.tv_nsec;
		nsec_getdata = t_getdata.tv_nsec - t_draw.tv_nsec;
		if(nsec_curses < 0) nsec_curses += 1000000000;
		if(nsec_readframe < 0) nsec_readframe += 1000000000;
		if(nsec_putframe < 0) nsec_putframe += 1000000000;
		if(nsec_draw < 0) nsec_draw += 1000000000;
		if(nsec_getdata < 0) nsec_getdata += 1000000000;
		
		//read current time
		clock_gettime(CLOCK_REALTIME, &gettime_now);
		time_difference = gettime_now.tv_nsec - start_time;
		if(time_difference < 0) time_difference += 1000000000;
		total_time_s += double(time_difference)/1000000000.0;
		total_frameset_time_s += double(time_difference)/1000000000.0;
		start_time = gettime_now.tv_nsec;
		float fr = float(double(UPDATERATE)/total_frameset_time_s);
		
		
	
		//print the screen
		if((i%UPDATERATE)==0)
		{			//draw the terminal window
			drawCurses(fr, nsec_curses, nsec_readframe, nsec_putframe, nsec_draw, nsec_getdata);
			total_frameset_time_s = 0;
		}

	}

	StopCamera();

	endwin();
	
	return 0;
}