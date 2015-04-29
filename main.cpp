#include "graphics.h"
#include "camera.h"
#include "common.h"
#include "window.h"
#include <stdio.h>
#include <curses.h>
#include <time.h>
#include <unistd.h>


int main(int argc, const char **argv)
{
	//init graphics and camera
	DBG("Start.");
	DBG("Initializing graphics and camera.");
	CCamera* cam = StartCamera(CAPTURE_WIDTH, CAPTURE_HEIGHT, CAPTURE_FPS, 1, false); //camera
	InitGraphics();
	DBG("Camera resolution: %dx%d", CAPTURE_WIDTH, CAPTURE_HEIGHT);
	
	DBG("Creating Textures.");
	//create YUV textures
	GfxTexture ytexture, utexture, vtexture, rgbtexture, rgblowtexture, outtexture, outlowtexture;		
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
	
#define NUMKEYS 3
	const char* keys[NUMKEYS] = {
		"s: show input and output snapshots in window (slow)",
		"w: save framebuffers to files (also slow)",
		"q: quit"
	};

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
	
		//print frame rate
		float fr = float(double(i+1)/total_time_s);
		if((i%30)==0)
		{
			mvprintw(0,0,"Framerate: %g",fr);
			mvprintw(2,0,"Controls:");
			int h=0;
			for(h=0; h<NUMKEYS; h++) mvprintw(h+3,0,keys[h]);
			move(20,0);
			refresh();
		}

	}

	StopCamera();

	endwin();
	
	return 0;
}