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

#define PI (3.141592653589793)

#define NUMKEYS 3
const char* keys[NUMKEYS] = {
	"s: show input and output snapshots in window (slow)",
	"w: save framebuffers to files (also slow)",
	"q: quit"
};

void drawCurses(float fr){
	updateStats();
	mvprintw(0,0,"Framerate: %g",fr);
	mvprintw(2,0,"Controls:");
	int h=0;
	for(h=0; h<NUMKEYS; h++) mvprintw(h+3,0,keys[h]);
	mvprintw(0,60,"CPU Total: %d",cputot_stats.load);
	mvprintw(1,65,"CPU1: %d",cpu1_stats.load);
	mvprintw(2,65,"CPU2: %d",cpu2_stats.load);
	mvprintw(3,65,"CPU3: %d",cpu3_stats.load);
	mvprintw(4,65,"CPU4: %d",cpu4_stats.load);
	move(20,0);
	refresh();
}

void initDeDonutTextures(GfxTexture *targetmap, GfxTexture* target){
	//this initializes the DeDonut texture, which is a texture that maps pixels of dedonuted to the donuted texture space (basically coordinate translation).

	//approximate dedonuted width based on circumference
	int width = (int)(PI*CAPTURE_WIDTH); 
	//approximate dedonuted height based on difference of radii of donut
	int height = (int)((g_conf.DONUTOUTERRATIO-g_conf.DONUTINNERRATIO)*CAPTURE_WIDTH); 
	target->CreateRGBA(CAPTURE_WIDTH,CAPTURE_HEIGHT);
	target->GenerateFrameBuffer();
	targetmap->CreateRGBA(CAPTURE_WIDTH,CAPTURE_HEIGHT);
	targetmap->GenerateFrameBuffer();
	
	//now, generate the pixels
	char* data = (char*)calloc(1,4*(size_t)width*(size_t)height);
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
			x = (float)i/(float)CAPTURE_WIDTH;
			y = (float)j/(float)CAPTURE_WIDTH;
			//calculations
			phi = x*2*PI;
			d = y*rout+rin;
			dx = d*sin(phi);
			dy = d*cos(phi);
			xout = cx+dx;
			yout = cy+dy;
			
			//store in pixel form
			data[(width*i+j)*4] = (int)(xout*255);
			data[(width*i+j)*4+1] = (int)(yout*255);
		}
	targetmap->SetPixels(data);
	free(data);	
	
	return;
}

int main(int argc, const char **argv)
{
	updateStats(); //baseline
	initConfig(); //get program settings

	//init graphics and camera
	DBG("Start.");
	DBG("Initializing graphics and camera.");
	CCamera* cam = StartCamera(CAPTURE_WIDTH, CAPTURE_HEIGHT, CAPTURE_FPS, 1, false); //camera
	InitGraphics();
	DBG("Camera resolution: %dx%d", CAPTURE_WIDTH, CAPTURE_HEIGHT);
	
	//set camera settings
	raspicamcontrol_set_metering_mode(cam->CameraComponent, METERINGMODE_AVERAGE);
	
	DBG("Creating Textures.");
	//create YUV textures
	GfxTexture ytexture, utexture, vtexture, rgbtexture, rgblowtexture, outtexture, outlowtexture, dedonuttexture, dedonutmap;		
	ytexture.CreateGreyScale(CAPTURE_WIDTH, CAPTURE_HEIGHT);
	utexture.CreateGreyScale(CAPTURE_WIDTH/2, CAPTURE_HEIGHT/2);
	vtexture.CreateGreyScale(CAPTURE_WIDTH/2, CAPTURE_HEIGHT/2);
	//for reading to CPU:
	GfxTexture yreadtexture,ureadtexture,vreadtexture;
	yreadtexture.CreateRGBA(CAPTURE_WIDTH,CAPTURE_HEIGHT);
	yreadtexture.GenerateFrameBuffer();
	ureadtexture.CreateRGBA(CAPTURE_WIDTH/2,CAPTURE_HEIGHT/2);
	ureadtexture.GenerateFrameBuffer();
	vreadtexture.CreateRGBA(CAPTURE_WIDTH/2,CAPTURE_HEIGHT/2);
	vreadtexture.GenerateFrameBuffer();
	//Main combined RGB textures
	rgbtexture.CreateRGBA(CAPTURE_WIDTH,CAPTURE_HEIGHT);
	rgbtexture.GenerateFrameBuffer();
	outtexture.CreateRGBA(CAPTURE_WIDTH,CAPTURE_HEIGHT);
	outtexture.GenerateFrameBuffer();
	//Subsampled RGB textures
	int lowh = (CAPTURE_HEIGHT/CAPTURE_WIDTH);
	if(!lowh) lowh = 1;
	lowh *= LOWRES_WIDTH;
	rgblowtexture.CreateRGBA(LOWRES_WIDTH, lowh);
	rgblowtexture.GenerateFrameBuffer();
	outlowtexture.CreateRGBA(LOWRES_WIDTH, lowh);
	outlowtexture.GenerateFrameBuffer();
	//DeDonut RGB textures
	initDeDonutTextures(&dedonutmap, &dedonuttexture);	
	
	//Start the processing loop.
	DBG("Starting process loop.");
	
	long int start_time;
	long int time_difference;
	struct timespec gettime_now;
	clock_gettime(CLOCK_REALTIME, &gettime_now);
	start_time = gettime_now.tv_nsec ;
	double total_time_s = 0;
	bool do_pipeline = false;

	initscr();      /* initialize the curses library */
	keypad(stdscr, TRUE);  /* enable keyboard mapping */
	nonl();         /* tell curses not to do NL->CR/NL on output */
	cbreak();       /* take input chars one at a time, no wait for \n */
	clear();
	nodelay(stdscr, TRUE);

	for(int i = 0; i < 3000; i++)
	{
		int ch = getch();
		if(ch != ERR)
		{
			SDL_Rect inrect = {0, 0, LOWRES_WIDTH, lowh};
			SDL_Rect outrect = {LOWRES_WIDTH, 0, LOWRES_WIDTH, lowh};
			switch(ch){
			case 's': //save framebuffers
				rgblowtexture.Show(&inrect);
				outlowtexture.Show(&outrect);
				break;
			case 'w': //save framebuffers
				//SaveFrameBuffer("tex_fb.png");
				rgbtexture.Save("./captures/tex_rgb.png");
				outtexture.Save("./captures/tex_out.png");
				dedonuttexture.Save("./captures/tex_dedonut.png");
				break;
			case 'q': //quit
				endwin();
				exit(1);
			}

			move(0,0);
			refresh();
		}
		
		//spin until we have a camera frame
		const void* frame_data; int frame_sz;
		while(!cam->BeginReadFrame(0,frame_data,frame_sz)) {};
		//lock the chosen frame buffer, and copy it directly into the corresponding open gl texture
		{
			const uint8_t* data = (const uint8_t*)frame_data;
			int ypitch = CAPTURE_WIDTH;
			int ysize = ypitch*CAPTURE_HEIGHT;
			int uvpitch = CAPTURE_WIDTH/2;
			int uvsize = uvpitch*CAPTURE_HEIGHT/2;
			//int upos = ysize+16*uvpitch;
			//int vpos = upos+uvsize+4*uvpitch;
			int upos = ysize;
			int vpos = upos+uvsize;
			//printf("Frame data len: 0x%x, ypitch: 0x%x ysize: 0x%x, uvpitch: 0x%x, uvsize: 0x%x, u at 0x%x, v at 0x%x, total 0x%x\n", frame_sz, ypitch, ysize, uvpitch, uvsize, upos, vpos, vpos+uvsize);
			ytexture.SetPixels(data);
			utexture.SetPixels(data+upos);
			vtexture.SetPixels(data+vpos);
			cam->EndReadFrame(0);
		}
		
		//begin frame
		BeginFrame();
			
		DrawYUVTextureRect(&ytexture,&utexture,&vtexture,-1.f,-1.f,1.f,1.f,&rgbtexture);
		
		//make output
		DrawOutRect(&rgbtexture, -1.0f, -1.0f, 1.0f, 1.0f, &outtexture); 
		
		//make dedonuted
		DrawDeDonutTextureRect(&rgbtexture, &dedonutmap, &dedonuttexture);
		
		//subsample
		DrawTextureRect(&rgbtexture, -1.0f, -1.0f, 1.0f, 1.0f, &rgblowtexture);
		DrawTextureRect(&outtexture, -1.0f, -1.0f, 1.0f, 1.0f, &outlowtexture);
			
		//these are just here so we can access the yuv data cpu side - opengles doesn't let you read grey ones cos they can't be frame buffers!
		DrawTextureRect(&ytexture,-1,-1,1,1,&yreadtexture);
		DrawTextureRect(&utexture,-1,-1,1,1,&ureadtexture);
		DrawTextureRect(&vtexture,-1,-1,1,1,&vreadtexture);
		
		EndFrame();
		
		//read current time
		clock_gettime(CLOCK_REALTIME, &gettime_now);
		time_difference = gettime_now.tv_nsec - start_time;
		if(time_difference < 0) time_difference += 1000000000;
		total_time_s += double(time_difference)/1000000000.0;
		start_time = gettime_now.tv_nsec;
	
		//print the screen
		float fr = float(double(i+1)/total_time_s);
		if((i%30)==0)
		{
			//draw the terminal window
			drawCurses(fr);
		}

	}

	StopCamera();

	endwin();
	
	return 0;
}