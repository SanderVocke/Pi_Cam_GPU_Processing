#include "graphics.h"
#include "camera.h"
#include "common.h"
#include <stdio.h>
#include <curses.h>
#include <time.h>
#include <unistd.h>


int main(int argc, const char **argv)
{
	//init graphics and camera
	DBG("Start.");
	DBG("Initializing graphics and camera.");
	CCamera* cam = StartCamera(CAPTURE_WIDTH, CAPTURE_HEIGHT, 15, 1, false); //10fps camera
	InitGraphics();
	DBG("Camera resolution: %dx%d", CAPTURE_WIDTH, CAPTURE_HEIGHT);
	
	DBG("Creating Textures.");
	//create YUV textures
	GfxTexture ytexture, utexture, vtexture;
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
			if(ch == 'q')
				break;
			else if(ch == 'a')
				break;
			else if(ch == 's')
				break;
			else if(ch == 'd')
				do_pipeline = !do_pipeline;
			else if(ch == 'w')
			{
				//SaveFrameBuffer("tex_fb.png");
				yreadtexture.Save("tex_y.png");
				ureadtexture.Save("tex_u.png");
				vreadtexture.Save("tex_v.png");
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
			mvprintw(0,0,"Framerate: %g\n",fr);
			move(0,0);
			refresh();
		}

	}

	StopCamera();

	endwin();
	
	return 0;
}