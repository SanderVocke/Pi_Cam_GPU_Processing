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
#include "fbmap.h"

#include "GLES2/gl2.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

#define PI (3.141592653589793)

FILE * logfile;
char messages[NUMDBG][300];
unsigned int msgi = 0;

int dWidth, dHeight; //width and height of "dedonuted" image
bool gHaveI2C = false;
bool showWindow = false;
bool renderScreen = true;
bool imageAvailable = true;
bool doImage = true;
bool doFilter = true;
bool doBehave = false;
bool doMap = false;

int dspeed[2];
direction_t ddir[2];

float redparams[4];
float blueparams[4];

int redadj = 0;
int blueadj = 0;
const char* redprops[] = {
	"Hmin", "Hmax", "Smin", "Vmin"
};
const char* blueprops[] = {
	"Hmin", "Hmax", "Smin", "Vmin"
};

//For post-processing on CPU
struct centroid{
	int x;
	int y;
};

struct box_x{
	int x_start;
	int x_stop;
};

struct box_y{
	int y_start;
	int y_stop;
};

struct object{
	int x_start;
	int x_stop;
	int y_start;
	int y_stop;
	int c_x;
	int c_y;
	int size_x;
	int size_y;
	bool confirmed;
};

struct centroid c_red[200];
struct centroid c_blue[200];
int red_centroid_total = 0;
int blue_centroid_total = 0;
int red_centroid_total_valid= 0;
int blue_centroid_total_valid= 0;

struct box_x  box_blue_x[200];
struct box_x  box_red_x[200];
struct box_y  box_red_y[200];
struct box_y  box_blue_y[200];
struct object object_blue[300];
struct object object_red[300];
struct object object_blue_valid[300];
struct object object_red_valid[300];

struct object target[3];
int targetfound;

int blue_x [30];
int blue_y [30];
int red_x [30];
int red_y [30];
int total_blue_x; 
int total_blue_y; 
int total_red_x; 
int total_red_y; 

//These time values are just for benchmarking and framerate calculations later during the loop.
long int start_time;
double total_frameset_time_s=0;
long int time_difference;
struct timespec gettime_now;
double total_time_s = 0;
bool do_pipeline = false;

CCamera* cam;

//more timing stuff to benchmark each step inside the loop separately.
struct timespec t_start, t_curses, t_readframe, t_putframe, t_draw, t_getdata, t_processdata;
long int nsec_curses, nsec_readframe, nsec_putframe, nsec_draw, nsec_getdata, nsec_processdata;

//these timing variables are for input keypress handling.
struct timespec t_up, t_down, t_left, t_right;
long int nsec_up, nsec_down, nsec_left, nsec_right;

//DEFINE CPU-SIDE HANDLES (CLASS OBJECTS) FOR ALL OPENGL TEXTURES WE WILL USE
GfxTexture ytexture, utexture, vtexture, rgbtexture, thresholdtexture, erodetexture, dilatetexture;
GfxTexture dedonutmap, horsumtexture1, horsumtexture2, versumtexture1, versumtexture2;
GfxTexture fileinputtexture;
GfxTexture lowdisptexture;
//add erode/dilate textures here!

#define UPDATERATE 10
#define MAX_COORDS 100
#define VER_STRETCH 3.0f

void analyzeResults(void); //analyze the results we got from GPU on the CPU
void drawCurses(float fr, long nsec_curses, long nsec_readframe, long nsec_putframe, long nsec_draw, long nsec_getdata, long nsec_processdata); //Draw the CURSES GUI
void initDeDonutTextures(GfxTexture *targetmap); //initialize the mapping look-up table texture for "de-donuting"
void showTexWindow(float lowh);
void doInput(void);
void renderDebugWindow(GfxTexture* render_target);
void drawBoxes(GfxTexture* render_target, float x0i, float y0i, float x1i, float y1i);
void drawBoxInRGB(int x0i, int y0i, int x1i, int y1i, float R, float G, float B);
void checkObject(struct object* obj, bool red);
void doBehaviour(void);
void doSetSpeedDir(motor_t motor, direction_t dir, int speed);
void doSetSpeed(motor_t motor, int speed);
void doSetDirection(motor_t motor, direction_t dir);

//This array of strings is just here to be printed to the terminal screen,
//telling the user what keys on the keyboard do what.
#define NUMKEYS 13
const char* keys[NUMKEYS] = {
	"arrow keys: move bot",
	"b: turn autonomous behaviour on/off",
	"s: show snapshot window",
	"w: save framebuffers",
	"r: turn HDMI live rendering on/off",
	"f: filtering on/off (erosion/dilation)",
	"i: use PNG input image instead of camera stream",
	"m: enable memory-mapped framebuffer",
	"t/g: change which thres param to tweak (red/blue resp.)",
	"y/h: course tweak of parameter (red/blue resp.)",
	"u/j: fine tweak of parameter (red/blue resp.)",
	"1/2/3/4: Exposure, Metering, AWB, FX resp."
	"q: quit"
};

////////////////////////////////////////////////////////
//MAIN PROGRAM FUNCTION (ENTRY POINT)
////////////////////////////////////////////////////////
int main(int argc, const char **argv)
{
	OPENLOG; //start log file.
	updateStats(); //baseline CPU usage stats.
	initConfig(); //get program settings from the config.txt file.
	for(int i =0; i<4; i++){
		redparams[i] = g_conf.REDPARAMS[i];
		blueparams[i] = g_conf.BLUEPARAMS[i];
	}
	if(startI2CMotor()) gHaveI2C = true; //Start I2C.

	//init graphics and camera
	DBG("Start.");
	DBG("Initializing graphics and camera.");
	cam = StartCamera(g_conf.CAPTURE_WIDTH, g_conf.CAPTURE_HEIGHT, g_conf.CAPTURE_FPS, 1, false); //Init camera object
	InitGraphics(); //Init OpenGL environment
	DBG("Camera resolution: %dx%d", g_conf.CAPTURE_WIDTH, g_conf.CAPTURE_HEIGHT);
	
	//set camera settings
	raspicamcontrol_set_metering_mode(cam->CameraComponent, MeteringMode_Enum[current_MeteringMode]);
	raspicamcontrol_set_awb_mode(cam->CameraComponent, AWB_Enum[current_AWB]);
	//raspicamcontrol_set_fx(cam->CameraComponent, FX_Enum[current_FX]);
	raspicamcontrol_set_exposure_mode(cam->CameraComponent, Exposure_Enum[current_Exposure]);
	
	//NOW ALLOCATE SPACE FOR ALL THESE TEXTURES
	DBG("Creating Textures.");
	//DeDonut RGB textures
	DBG("Max texture size: %d", GL_MAX_TEXTURE_SIZE);
	initDeDonutTextures(&dedonutmap); //(Create and also fill in its values, see function)
	
	//input image texture: if we are rendering from an input image instead of the camera, the data is in this.
	unsigned char * inputbuf;
	unsigned int w,h;
	LodePNGState lstate;
	lodepng_decode32_file(&inputbuf, &w, &h, "./inputs/input.png");
	if(w>0 && h>0){
		fileinputtexture.CreateRGBA(w,h,(const void*)inputbuf,(GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
		imageAvailable = true;
	}
	
	//create YUV textures
	//these textures will store the image data coming from the camera.
	//the camera records in YUV color space. Each color component is stored in a SEPARATE buffer, which is why
	//we have three separate textures for them.
	ytexture.CreateGreyScale(g_conf.CAPTURE_WIDTH, g_conf.CAPTURE_HEIGHT, NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	utexture.CreateGreyScale(g_conf.CAPTURE_WIDTH/2, g_conf.CAPTURE_HEIGHT/2, NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	vtexture.CreateGreyScale(g_conf.CAPTURE_WIDTH/2, g_conf.CAPTURE_HEIGHT/2, NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	
	//Main combined RGB textures
	//rgbtexture will hold the result of transforming the separate y,u,v textures into a single RGBA format texture.
	//in addition to that, rgbtexture is already unrolled into a wide panoramic image
	
	rgbtexture.CreateRGBA(dWidth,dHeight,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	rgbtexture.GenerateFrameBuffer();
	
	//thresholdtexture holds the result of performing thresholding (and possibly other filtering steps)
	thresholdtexture.CreateRGBA(dWidth,dHeight,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	thresholdtexture.GenerateFrameBuffer();
	
	//Morphological filtering textures erode and dilate
	erodetexture.CreateRGBA(dWidth,dHeight,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	erodetexture.GenerateFrameBuffer();
	dilatetexture.CreateRGBA(dWidth,dHeight,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	dilatetexture.GenerateFrameBuffer();
	
	//the textures below hold the summation results. It is a two-stage summation: therefore there are two textures for this with different sizes.
	//the two-stage approach is because of the GLSL limitation on Pi that allows only reading 64 pixels max per shader.
	horsumtexture1.CreateRGBA((dWidth/64)+1,dHeight,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	horsumtexture1.GenerateFrameBuffer();
	horsumtexture2.CreateRGBA(1,dHeight,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	horsumtexture2.GenerateFrameBuffer();
	versumtexture1.CreateRGBA(dWidth,(dHeight/64)+1,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	versumtexture1.GenerateFrameBuffer();
	versumtexture2.CreateRGBA(dWidth,1,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	versumtexture2.GenerateFrameBuffer();
	
	//initialize the new textures here!
	

	//showWindowd RGB textures
	//these textures are just low-resolution versions of the above. they are used when we want to get a preview window: transfering the
	//full-size textures takes too much time so they are first down-sampled.
	int lowh = (int)(0.7f * g_conf.LOWRES_WIDTH);
	if(!lowh) lowh = 1;
	lowdisptexture.CreateRGBA(g_conf.LOWRES_WIDTH, lowh, NULL, (GLfloat) GL_LINEAR, (GLfloat) GL_CLAMP_TO_EDGE);
	lowdisptexture.GenerateFrameBuffer();
	
	
	//Start the processing loop.
	DBG("Starting process loop.");

	//Init the CURSES window.
	initscr();      /* initialize the curses library */
	keypad(stdscr, TRUE);  /* enable keyboard mapping */
	nonl();         /* tell curses not to do NL->CR/NL on output */
	cbreak();       /* take input chars one at a time, no wait for \n */
	clear();
	nodelay(stdscr, TRUE);
	
	//baseline times.
	clock_gettime(CLOCK_REALTIME, &t_up);
	t_down=t_left=t_right=t_up;
	clock_gettime(CLOCK_REALTIME, &gettime_now);
	start_time = gettime_now.tv_nsec ;

	//MAIN PROCESSING LOOP
	for(long i = 0;; i++)
	{
		clock_gettime(CLOCK_REALTIME, &t_start); //for benchmarking.
		
		//If the user pressed 's' to show the current frame (during the previous loop iteration),
		//this boolean showWindow will have been set and the frames all rendered to low-res versions
		//for easy grabbing. Now we will actually show them in an X window using SDL.
		if(showWindow){
			showTexWindow(lowh); //show it			
			//set to false again until the user may press 's' again in the future.
			showWindow = false;
		}

		//INPUT HANDLING OF KEYBOARD KEYS
		doInput(); //handles input.
		
		clock_gettime(CLOCK_REALTIME, &t_curses); //for benchmarking
		
		//PART THAT GRABS A FRAME FROM THE CAMERA
		//spin until we have a camera frame
		const void* frame_data; int frame_sz;
		//the camera is set to a certain framerate. Therefore, by the time we get here, the next frame may not be ready yet.
		//therefore this while loop loops until a frame is ready, and when it is,  grabs it and puts it in the frame_data buffer.
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
		//now we have the frame from the camera in the CPU memory in the 'data'  buffer. Now we have to upload it to the GPU as textures using the
		//SetPixels() function provided by the OpenGL API.
		ytexture.SetPixels(data);
		utexture.SetPixels(data+upos);
		vtexture.SetPixels(data+vpos);
		cam->EndReadFrame(0);
		
		clock_gettime(CLOCK_REALTIME, &t_putframe); //benchmarking		
		
		//PART THAT INSTRUCTS OPENGL TO START EXECUTING THE SHADER CODE (.glsl files) ON THE GPU
		//begin frame: a call from the graphics.h/cpp module that starts the rendering pipeline for this frame.
		BeginFrame();
			
		if(!doImage){ //normal case: render from the camera stream.
			DrawYUVTextureRect(&ytexture,&utexture,&vtexture,&dedonutmap,-1.f,-1.f,1.f,1.f,&rgbtexture); //separate Y, U, V donut textures to RGBA panorama texture.
		}
		else{ //render from the static PNG image texture we made.
			DrawTextureRect(&fileinputtexture, -1.0f, -1.0f, 1.0f, 1.0f, &rgbtexture);
		}
		
		//make thresholded
		DrawThresholdRect(&rgbtexture, -1.0f, -1.0f, 1.0f, 1.0f, 
			redparams[0], redparams[1], redparams[2], redparams[3],
			blueparams[0], blueparams[1], blueparams[2], blueparams[3],
			&thresholdtexture); //perform thresholding
		
		if(doFilter){
			// Erode and dilate 
			/** Morphological Opening */
			DrawErode(&thresholdtexture, -1.0f, -1.0f, 1.0f, 1.0f, &dilatetexture); //perform erosion [ in the Opening phase, the temporary outputs 
			DrawDilate(&dilatetexture, -1.0f, -1.0f, 1.0f, 1.0f, &erodetexture); //perform dilation		are stored in the wrong texture, for texture re-use]
			/** Morphological Closing */
			DrawDilate(&erodetexture, -1.0f, -1.0f, 1.0f, 1.0f, &dilatetexture); //perform dilation	  [ in the Closing phase, they are in right order ]
			DrawErode(&dilatetexture, -1.0f, -1.0f, 1.0f, 1.0f, &erodetexture); //perform erosion
		}
		else{
			DrawTextureRect(&thresholdtexture, -1.0f, -1.0f, 1.0f, 1.0f, &erodetexture);
		}
		//sum rows/columns
		DrawHorSum1(&erodetexture, -1.0f, -1.0f, 1.0f, 1.0f, &horsumtexture1); //first horizontal summer stage
		DrawHorSum2(&horsumtexture1, -1.0f, -1.0f, 1.0f, 1.0f, &horsumtexture2); //second (final) horizontal summer stage
		DrawVerSum1(&erodetexture, -1.0f, -1.0f, 1.0f, 1.0f, &versumtexture1); //first vertical summer stage
		DrawVerSum2(&versumtexture1, -1.0f, -1.0f, 1.0f, 1.0f, &versumtexture2); //second (final) vertical summer stage		
		
		
		Finish(); //for accurate benchmarking
		
		clock_gettime(CLOCK_REALTIME, &t_draw); //benchmarking
		
		//DATA RETREIVAL FROM GPU
		//get summed data back to host (CPU side)
		versumtexture2.Get();
		horsumtexture2.Get();
		versumtexture1.Get();
		horsumtexture1.Get();
		
		clock_gettime(CLOCK_REALTIME, &t_getdata); //benchmarking
		
		//DATA ANALYSIS ON CPU
		analyzeResults();
		
		//BEHAVIOUR
		if(doBehave) doBehaviour();
		else{
			doSetSpeed(RIGHT_MOTOR,0);
			doSetSpeed(LEFT_MOTOR, 0);
		}
		
		//if 's' was pressed, showWindow will be true. Then we need to render our textures onto low-resolution versions for faster capturing back to CPU.
		if(showWindow){
			renderDebugWindow(&lowdisptexture); //render to texture
		}		
		else if(renderScreen){//if on-screen rendering to HDMI is active, draw all textures in the pipeline to the screen framebuffer as well.
			renderDebugWindow(NULL); //render to screen
		}
		if(doMap){ //write to fb map
			//DBG("Writing to memmap @ %lu.", (unsigned long)getMapAddr());
			if(!showWindow) renderDebugWindow(&lowdisptexture); //render to texture
			lowdisptexture.GetTo(getMapAddr());
			//*((char*)getMapAddr()) = 'a';
		}
		
		EndFrame();
		
		clock_gettime(CLOCK_REALTIME, &t_processdata); //for benchmarking.		
		
		//do all benchmarking time calculations
		nsec_curses = t_curses.tv_nsec - t_start.tv_nsec;
		nsec_readframe = t_readframe.tv_nsec - t_curses.tv_nsec;
		nsec_putframe = t_putframe.tv_nsec - t_readframe.tv_nsec;
		nsec_draw = t_draw.tv_nsec - t_putframe.tv_nsec;
		nsec_getdata = t_getdata.tv_nsec - t_draw.tv_nsec;
		nsec_processdata = t_processdata.tv_nsec - t_getdata.tv_nsec;
		if(nsec_curses < 0) nsec_curses += 1000000000;
		if(nsec_readframe < 0) nsec_readframe += 1000000000;
		if(nsec_putframe < 0) nsec_putframe += 1000000000;
		if(nsec_draw < 0) nsec_draw += 1000000000;
		if(nsec_getdata < 0) nsec_getdata += 1000000000;
		if(nsec_processdata < 0) nsec_processdata += 1000000000;
		
		//read current time value
		clock_gettime(CLOCK_REALTIME, &gettime_now);
		time_difference = gettime_now.tv_nsec - start_time;
		if(time_difference < 0) time_difference += 1000000000;
		total_time_s += double(time_difference)/1000000000.0;
		total_frameset_time_s += double(time_difference)/1000000000.0;
		start_time = gettime_now.tv_nsec;
		float fr = float(double(UPDATERATE)/total_frameset_time_s);
			
		//print the screen once every so many iterations
		if((i%UPDATERATE)==0)
		{			//draw the terminal window
			drawCurses(fr, nsec_curses, nsec_readframe, nsec_putframe, nsec_draw, nsec_getdata, nsec_processdata);
			total_frameset_time_s = 0;
		}

	}

	//if we get here, the main loop was exited for some reason.
	StopCamera(); //stop camera
	endwin(); //close window of CURSES
	CLOSELOG; //close log file
	
	return 0;
}

void renderDebugWindow(GfxTexture* render_target){
	
	if(render_target == NULL){
		DrawTextureRect(&rgbtexture, 0.8f, 1.0f, -1.0f, 0.2f, render_target);
		//DrawTextureRect(&thresholdtexture, 0.8f, -0.2f, -1.0f, -1.0f, render_target);
		//DrawTextureRect(&erodetexture, 0.8f, 1.0f, -1.0f, 0.2f, render_target); //draw eroded where input was before
		DrawTextureRect(&erodetexture, 0.8f, -0.2f, -1.0f, -1.0f, render_target); //draw eroded where thresholded was before
		DrawTextureRect(&horsumtexture1, 0.95f, -0.2f, 0.8f, -1.0f,  render_target);
		DrawTextureRect(&horsumtexture2, 1.0f, -0.2f, 0.95f, -1.0f, render_target);
		DrawTextureRect(&versumtexture1, 0.8f, 0.0f, -1.0f, -0.2f, render_target);
		DrawTextureRect(&versumtexture2, 0.8f, 0.15f, -1.0f, 0.05f, render_target);	
		DrawRangeRect(0.8f, 1.0f, 1.0f, 0.6f,
			redparams[0], redparams[1], redparams[2], redparams[3],
			render_target);
		DrawRangeRect(0.8f, 0.6f, 1.0f, 0.2f,
			blueparams[0], blueparams[1], blueparams[2], blueparams[3],
			render_target);
		drawBoxes(render_target, 0.8f, 1.0f, -1.0f, 0.2f);
	}
	else{
		//drawBoxes(&rgbtexture, -1.0f,-1.0f,1.0f,1.0f);
		DrawTextureRect(&rgbtexture, -1.0f, 0.2f, 0.8f, 1.0f, render_target);
		//DrawTextureRect(&thresholdtexture, -1.0f, -1.0f, 0.8f, -0.2f, render_target);
		//DrawTextureRect(&erodetexture, -1.0f, 0.2f, 0.8f, 1.0f, render_target); //draw eroded where input was before
		DrawTextureRect(&erodetexture, -1.0f, -1.0f, 0.8f, -0.2f, render_target); //draw eroded where thresholded was before
		DrawTextureRect(&horsumtexture1, 0.8f, -1.0f, 0.95f, -0.2f,  render_target);
		DrawTextureRect(&horsumtexture2, 0.95f, -1.0f, 1.0f, -0.2f, render_target);
		DrawTextureRect(&versumtexture1, -1.0f, -0.2f, 0.8f, 0.0f, render_target);
		DrawTextureRect(&versumtexture2, -1.0f, 0.05f, 0.8f, 0.15f, render_target);	
		DrawRangeRect(0.8f, 0.2f, 1.0f, 0.6f,
			redparams[0], redparams[1], redparams[2], redparams[3],
			render_target);
		DrawRangeRect(0.8f, 0.6f, 1.0f, 1.0f,
			blueparams[0], blueparams[1], blueparams[2], blueparams[3],
			render_target);
		drawBoxes(render_target, -1.0f, 0.2f, 0.8f, 1.0f );
	}
}


//this is a function which will be called from the main() program's main loop,
//updating the "text GUI" which is visible when running the program over SSH.
#define STATSLINE 0
#define STATSCOL 0
#define SPEEDLINE (STATSLINE + 7)
#define SPEEDCOL 0
#define BEHAVELINE (SPEEDLINE + 3)
#define BEHAVECOL 0
#define I2CLINE (BEHAVELINE+1)
#define I2CCOL 0
#define HDMILINE (I2CLINE+1)
#define HDMICOL 0
#define IMGLINE (HDMILINE+1)
#define IMGCOL 0
#define FILTERLINE (IMGLINE + 1)
#define FILTERCOL 0
#define BENCHLINE (FILTERLINE + 2)
#define BENCHCOL 0
#define BLUELINE (BENCHLINE + 8)
#define BLUECOL 0
#define REDLINE BLUELINE
#define REDCOL 40
#define CONTROLLINE 0
#define CONTROLCOL 40
#define SETTINGSLINE (CONTROLLINE + 3 + NUMKEYS)
#define SETTINGSCOL CONTROLCOL
#define THRESLINE (REDLINE + 2)
#define THRESCOL 0
#define DBGLINE (THRESLINE + 3)
#define DBGCOL 0
void drawCurses(float fr, long nsec_curses, long nsec_readframe, long nsec_putframe, long nsec_draw, long nsec_getdata, long nsec_processdata){
	clear();
	
	//Update CPU usage stats
	updateStats();
	
	//Print framerate and CPU usage stats
	mvprintw(STATSLINE+0,STATSCOL,"Framerate: %g  ",fr);	
	mvprintw(STATSLINE+1,STATSCOL,"CPU Total: %d   ",cputot_stats.load);
	mvprintw(STATSLINE+2,STATSCOL,"CPU1: %d  ",cpu1_stats.load);
	mvprintw(STATSLINE+3,STATSCOL,"CPU2: %d  ",cpu2_stats.load);
	mvprintw(STATSLINE+4,STATSCOL,"CPU3: %d  ",cpu3_stats.load);
	mvprintw(STATSLINE+5,STATSCOL,"CPU4: %d  ",cpu4_stats.load);
	
	//Print robot motor states.
	mvprintw(SPEEDLINE,SPEEDCOL,"Left Speed: %d / %d  ",
		getDirection(LEFT_MOTOR)==FORWARD ? getSpeed(LEFT_MOTOR) : -1*((int)getSpeed(LEFT_MOTOR)),
		ddir[(int)LEFT_MOTOR]==FORWARD ? dspeed[(int)LEFT_MOTOR] : -1*dspeed[(int)LEFT_MOTOR]);
	mvprintw(SPEEDLINE+1,SPEEDCOL,"Right Speed: %d / %d   ",
		getDirection(RIGHT_MOTOR)==FORWARD ? getSpeed(RIGHT_MOTOR) : -1*((int)getSpeed(RIGHT_MOTOR)),
		ddir[(int)RIGHT_MOTOR]==FORWARD ? dspeed[(int)RIGHT_MOTOR] : -1*dspeed[(int)RIGHT_MOTOR]);
	
	//Print whether autonomously moving
	if(doBehave) mvprintw(BEHAVELINE, BEHAVECOL, "BRAIN ON ");
	else mvprintw(BEHAVELINE, BEHAVECOL, "BRAIN OFF");
	
	//Print I2C driver state (ON or FAIL)
	if(gHaveI2C) mvprintw(I2CLINE,I2CCOL,"I2C ON  ");
	else mvprintw(I2CLINE,I2CCOL,"I2C FAIL");
	
	//Print whether live HDMI rendering is on
	if(renderScreen) mvprintw(HDMILINE,HDMICOL, "HDMI ON     ");
	else mvprintw(HDMILINE,HDMICOL, "HDMI OFF     ");
	
	//Print whether we are rendering an image or a camera stream
	if(doImage) mvprintw(IMGLINE,IMGCOL, "Input from: './inputs/input.png'     ");
	else mvprintw(IMGLINE,IMGCOL,"Input from: CAMERA                          ");
	
	//Print whether we are filtering
	if(doFilter) mvprintw(FILTERLINE, FILTERCOL, "Filtering: ON    ");
	else  mvprintw(FILTERLINE, FILTERCOL, "Filtering: OFF    ");
	
	//Print benchmarking results
	mvprintw(BENCHLINE,BENCHCOL,"msec Curses: %d   ",nsec_curses/1000000);
	mvprintw(BENCHLINE+1,BENCHCOL,"msec Readframe: %d   ",nsec_readframe/1000000);
	mvprintw(BENCHLINE+2,BENCHCOL,"msec Putframe: %d   ",nsec_putframe/1000000);
	mvprintw(BENCHLINE+3,BENCHCOL,"msec Draw: %d   ",nsec_draw/1000000);
	mvprintw(BENCHLINE+4,BENCHCOL,"msec Getdata: %d   ",nsec_getdata/1000000);
	mvprintw(BENCHLINE+5,BENCHCOL,"msec Processdata: %d        ",nsec_processdata/1000000);
	
	//print the objects we found (for now, x and y axis separately!)
	int i;
	mvprintw(BLUELINE, BLUECOL, "Red blobs: %d Blue blobs: %d Targets: %d      ", red_centroid_total, blue_centroid_total, targetfound);
	//for(i=0; i<20; i++){
		//if(i<total_blue_x) mvprintw(i+BLUELINE+1,BLUECOL, "X found: (%d,?)      ",blue_x[i]);
		//else if(i<(total_blue_x+total_blue_y)) mvprintw(i+BLUELINE+1,BLUECOL,  "Y found: (?,%d)      ",blue_y[i-total_blue_x]);
		//else mvprintw(i+BLUELINE+1,BLUECOL, "(not found)                  ");
	//}
	//mvprintw(REDLINE, REDCOL, "Red objects found: %d      ", red_centroid_total);
	//for(i=0; i<20; i++){
		//if(i<total_red_x) mvprintw(REDLINE+i+1,REDCOL, "X found: (%d,?)      ",red_x[i]);
		//else if(i<(total_red_x+total_red_y)) mvprintw(REDLINE+i+1,REDCOL, "Y found: (?,%d)      ",red_y[i-total_red_x]);
		//else mvprintw(REDLINE+i+1,REDCOL,"(not found)                 ");
	//}
	
	//print the HSV thresholding thresholds
	mvprintw(THRESLINE, THRESCOL, "Red Hmin: %f Hmax: %f Smin: %f Vmin: %f Adjusting: %s", redparams[0], redparams[1], redparams[2], redparams[3], redprops[redadj]);
	mvprintw(THRESLINE+1, THRESCOL, "Blue Hmin: %f Hmax: %f Smin: %f Vmin: %f Adjusting: %s", blueparams[0], blueparams[1], blueparams[2], blueparams[3], blueprops[blueadj]);
	
	//Print the keyboard control instructions
	mvprintw(CONTROLLINE, CONTROLCOL,"Controls:");
	int h=0;
	for(h=0; h<NUMKEYS; h++) mvprintw(CONTROLLINE+h+2,CONTROLCOL,keys[h]);
	
	//Print the camera settings.
	mvprintw(SETTINGSLINE, SETTINGSCOL, "EXPOSURE: %s", Exposure_Name_Enum[current_Exposure]);
	mvprintw(SETTINGSLINE+1, SETTINGSCOL, "AWB: %s", AWB_Name_Enum[current_AWB]);
	mvprintw(SETTINGSLINE+2, SETTINGSCOL, "FX: %s", FX_Name_Enum[current_FX]);
	mvprintw(SETTINGSLINE+3, SETTINGSCOL, "METERING: %s", MeteringMode_Name_Enum[current_MeteringMode]);
	
	//print debug messages
	mvprintw(DBGLINE, DBGCOL, "LAST %d DEBUG MESSAGES:", NUMDBG);
	mvprintw(DBGLINE+1, DBGCOL, "---------------------------------------------");
	for(i=0; i<NUMDBG; i++){
		mvprintw(DBGLINE+2+i,DBGCOL,"-                                              ");
	}
	for(i=0; i<NUMDBG; i++){
		mvprintw(DBGLINE+2+i,DBGCOL+2,messages[(msgi+i)%NUMDBG]);
	}
	
	//finally draw all of this to the screen.
	refresh();
}


//In order to transform the donut-shaped image into a panorama we use a "mapping texture",
//which is just a huge look-up table that specifies which pixel to map where. This function makes 
//the calculations to (resolution-independently) fill in this mapping texture.
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

//function that processes the GPU results into actual coordinates of target
void analyzeResults(void){	
	int a;
	int a1,a2, sum_blue, sum_red;
	int x_bhigh[40];
	int x_blow[40];
	int x_rhigh[40];
	int x_rlow[40];
	total_red_x = 0; 
	int set_blue = 1;
	int set_red =1;
	int k_blue=0;
	int k_red=0;
	total_blue_x  = 0;
	unsigned char * verptr = (unsigned char*)versumtexture2.image;
	for(int j=0; j< (versumtexture2.Width); j++)
	{   
		a1 = verptr[j*4 +2];
		if (set_blue ==1)
		{
			if(a1>0)
			{   
				x_bhigh[k_blue] =j;
				box_blue_x[k_blue].x_start=j;
				
				set_blue =0;
			}
	     }
		if (set_blue ==0)
		{
			if ((a1==0)||(j == (versumtexture2.Width) -1))
			{   
				total_blue_x++;
				x_blow[k_blue] =j;
				box_blue_x[k_blue].x_stop=j;
				set_blue =1;
			
				sum_blue = (x_bhigh[k_blue] + ( x_blow[k_blue] -x_bhigh[k_blue] )/2);
				blue_x[k_blue]=sum_blue;
				//DBG(" * x_blue[%d]  = %d\n", total_blue_x,sum_blue);
				k_blue++;
			}
		}
		  
		//red x coordinates
		a2 = verptr[j*4];
		if (set_red ==1)
		{
			if(a2>0)
			{   
				x_rhigh[k_red] =j;
				box_red_x[k_red].x_start=j;
				set_red =0;
			}
	     }
		if (set_red ==0)
		{
			if ((a2==0)||(j == (versumtexture2.Width) -1))
			{   
				total_red_x++;
				x_rlow[k_red] =j;
				box_red_x[k_red].x_stop=j;
				set_red =1;
			
				sum_red = (x_rhigh[k_red] + ( x_rlow[k_red] -x_rhigh[k_red] )/2);
				red_x[k_red]=sum_red;
				//DBG(" * x_red[%d]  = %d\n", total_red_x,sum_red);
				//DBG(" * x_rlow[%d]  = %d\n", total_red_y,x_rlow[k_red] );
				//DBG(" * x_rhigh[%d]  = %d\n", total_red_y,x_rhigh[k_red] );
				//DBG(" * x_rlow[%d]  = %d\n", total_red_y, box_red_x[k_red].x_stop);
				//DBG(" * x_rhigh[%d]  = %d\n", total_red_y, box_red_x[k_red].x_start);
				k_red++;
			}
		 }

	}	
	
	set_blue = 1;
	set_red =1;
	k_blue=0;
	k_red=0;
	int y_bhigh[15];
	int y_blow[15];
	int y_rhigh[15];
	int y_rlow[15];
	total_red_y = 0; 
	total_blue_y= 0; 
	unsigned char * horptr = (unsigned char*)horsumtexture2.image;
	for(int j=0; j< (horsumtexture2.Height); j++)
	{
		
		a1 = horptr[j*4 +2];;
		if (set_blue ==1)
		{
			if(a1>0)
			{   			
				y_bhigh[k_blue] =j;
				box_blue_y[k_blue].y_start=j;
				set_blue =0;
			}
	    }

		if (set_blue ==0)
		{
			if ((a1==0)||(j == (horsumtexture2.Height)-1))
			{   
				total_blue_y++;
				y_blow[k_blue] =j;
				box_blue_y[k_blue].y_stop=j;
				set_blue =1;
				sum_blue = (y_bhigh[k_blue] + ( y_blow[k_blue] - y_bhigh[k_blue] )/2);
				blue_y[k_blue] =sum_blue;
				//DBG(" * y_blue[%d]  = %d\n", total_blue_y,sum_blue);
				
				k_blue++;
			}
	
	 	}
		 
		//red 
		a2 = horptr[j*4];;
		if (set_red ==1)
		{
	       if(a2>0)
		   {   
		    	y_rhigh[k_red] =j;
				box_red_y[k_red].y_start=j;
				set_red =0;
           }
	    }

		if (set_red ==0)
		{
			if ((a2==0)||(j == (horsumtexture2.Height)-1))
			{   
				total_red_y++;
				y_rlow[k_red] =j;
				box_red_y[k_red].y_stop=j;
				set_red =1;
				sum_red = (y_rhigh[k_red] + ( y_rlow[k_red] - y_rhigh[k_red] )/2);
				red_y[k_red] =sum_red;
				//DBG(" * y_red[%d]  = %d\n", total_red_y,sum_red);
				//DBG(" * y_rlow[%d]  = %d\n", total_red_y,y_rlow[k_red] );
				//DBG(" * y_rhigh[%d]  = %d\n", total_red_y,y_rhigh[k_red] );
				//DBG(" * y_rlow[%d]  = %d\n", total_red_y,box_red_y[k_red].y_stop);
				//DBG(" * y_rhigh[%d]  = %d\n", total_red_y,box_red_y[k_red].y_start);
				//DBG(" * k_value[%d]  = %d\n", total_red_y,k_red);
			
				k_red++;
			}

	 	}
	}
	
	int g,h;
	red_centroid_total =0; 
		for (g=0; g<total_red_x;g++)
		{   
			for (h=0; h<total_red_y;h++)
			
			{
				object_red[red_centroid_total].c_x = red_x[g];
				object_red[red_centroid_total].c_y = red_y[h];
				object_red[red_centroid_total].size_x = box_red_x[g].x_stop -  box_red_x[g].x_start ;
				object_red[red_centroid_total].size_y = box_red_y[h].y_stop- box_red_y[h].y_start ;
				
				object_red[red_centroid_total].x_start = box_red_x[g].x_start;
				object_red[red_centroid_total].x_stop =  box_red_x[g].x_stop;
				object_red[red_centroid_total].y_start = box_red_y[h].y_start;
				object_red[red_centroid_total].y_stop= box_red_y[h].y_stop;
				object_red[red_centroid_total].confirmed = false;
				
				checkObject(&(object_red[red_centroid_total]), true);
				
				
				
				//DrawTextureRect(&rgbtexture, 0.8f, 1.0f, -1.0f, 0.2f, NULL);
				//DBG("%f %f %f %f \n", x0,y0,x1,y1);
				//DBG("%d \n", object_red[red_centroid_total].x_start);
				//DBG("%d \n", object_red[red_centroid_total].x_stop);
				//DBG("%d \n", object_red[red_centroid_total].y_start);
				//DBG("%d \n", object_red[red_centroid_total].y_start);				
				//DrawBox(0.0f,0.0f,0.1f,0.1f,1.0f,1.0f,0.0f);
				red_centroid_total++;
			}
		}
		 //DBG(" * Total red centroids %d\n",red_centroid_total);
		
		
		
	blue_centroid_total =0; 
		for (g=0; g<total_blue_x;g++)
		{   
			for (h=0; h<total_blue_y;h++)
			
			{
				object_blue[blue_centroid_total].c_x = blue_x[g];
				object_blue[blue_centroid_total].c_y = blue_y[h];
				object_blue[blue_centroid_total].size_x = box_blue_x[g].x_stop -  box_blue_x[g].x_start ;
				object_blue[blue_centroid_total].size_y = box_blue_y[h].y_stop- box_blue_y[h].y_start ;
				object_blue[blue_centroid_total].x_start = box_blue_x[g].x_start;
				object_blue[blue_centroid_total].x_stop =  box_blue_x[g].x_stop;
				object_blue[blue_centroid_total].y_start = box_blue_y[h].y_start;
				object_blue[blue_centroid_total].y_stop= box_blue_y[h].y_stop;
				object_blue[blue_centroid_total].confirmed = false;
				checkObject(&(object_blue[blue_centroid_total]), false);
				blue_centroid_total++;
				
			}
		}

		//DBG(" * Total blue centroids %d\n",blue_centroid_total  );
		
		    blue_centroid_total_valid =0;
			for (g=0; g< blue_centroid_total ;g++)
			
			{    
			   if (object_blue[g].confirmed == true)
			   {
				object_blue_valid[blue_centroid_total_valid].x_start       = object_blue[g].x_start;
				object_blue_valid[blue_centroid_total_valid].x_stop       = object_blue[g].x_stop;
				object_blue_valid[blue_centroid_total_valid].y_start       = object_blue[g].y_start;
				object_blue_valid[blue_centroid_total_valid].y_stop       = object_blue[g].y_stop;
				object_blue_valid[blue_centroid_total_valid].c_x            = object_blue[g].c_x;
				object_blue_valid[blue_centroid_total_valid].c_y            = object_blue[g].c_y;
				object_blue_valid[blue_centroid_total_valid].size_x       = object_blue[g]. size_x;
				object_blue_valid[blue_centroid_total_valid].size_y       = object_blue[g].size_y;
				object_blue_valid[blue_centroid_total_valid].confirmed = true;   
			   }
				
				
				blue_centroid_total_valid++;
			}
			
			red_centroid_total_valid =0;
			for (g=0; g< red_centroid_total ;g++)
			
			{    
			   if (object_red[g].confirmed == true)
			   {
				object_red_valid[red_centroid_total_valid].x_start  =  object_red[g].x_start;
				object_red_valid[red_centroid_total_valid].x_stop  =  object_red[g].x_stop; 
				object_red_valid[red_centroid_total_valid].y_start =   object_red[g].y_start;
				object_red_valid[red_centroid_total_valid].y_stop  =  object_red[g].y_stop; 
				object_red_valid[red_centroid_total_valid].c_x       =  object_red[g].c_x; 
				object_red_valid[red_centroid_total_valid].c_y       =  object_red[g].c_y;  
				object_red_valid[red_centroid_total_valid].size_x  =  object_red[g]. size_x;
				object_red_valid[red_centroid_total_valid].size_y  =  object_red[g].size_y;
				object_red_valid[red_centroid_total_valid].confirmed  = true;   
			   }
				
				
				red_centroid_total_valid++;
			}
		
		
		targetfound=0;
		for( int i = 0; i< red_centroid_total_valid; i++ )
        { 
	     for( int j = 0; j<blue_centroid_total_valid; j++ )
		 {
			 if 
				 (
				                               ( 
											        ( object_red_valid[i].c_y  < object_blue_valid[j].c_y )
												   &&
                                                   ( object_red_valid[i].c_y  + (object_red_valid[i].size_y) >object_blue_valid[j].c_y )											   
											  
											  )
											   &&
											   (
				                               ( ( object_red_valid[i].c_x   < object_blue_valid[j].c_x )&& (object_blue_valid[j].c_x < (object_red_valid[i].c_x  +(object_red_valid[i].size_x)))) ||
											   (((object_red_valid[i].c_x - (object_red_valid[i].size_x))<object_blue_valid[j].c_x )&&(object_blue_valid[j].c_x<object_red_valid[i].c_x   ))
											   )
											   
				 )
				
				{	
				
				if(targetfound<3){
				
				     target[targetfound].x_start  =  object_red[i].x_start;
				     target[targetfound].x_stop  =  object_red[i].x_stop; 
				     target[targetfound].y_start =   object_red[i].y_start;
				     target[targetfound].y_stop  =  object_red[i].y_stop; 
				     target[targetfound].c_x       =  object_red[i].c_x; 
				     target[targetfound].c_y       =  object_red[i].c_y;  
				     target[targetfound].size_x  =  object_red[i]. size_x;
				     target[targetfound].size_y  =  object_red[i].size_y;
				     target[targetfound].confirmed  = true;   
				     targetfound++;
				
				     break;
				    }
				}
         
			
			
				
				
			    //DBG(" * Contour_blue[%d] - Cx(%d) = %d \n", j,j, c_blue[j].x );
			    //DBG(" * Contour_blue[%d] - Cy(%d) = %d \n", j,j, c_blue[j].y );
			    //DBG(" * Contour_red[%d] - Cx(%d) = %d \n", i,i, c_red[i].x );
	            //DBG(" * Contour_red[%d] - Cy(%d) = %d \n", i,i, c_red[i].y );
			  
			}

		 }
	//DBG("targets found: %d", targetfound);
	//DBG("1st target %d %d", target[0].c_x, target[0].c_y);
	   //DBG(" * total possibilities %d \n", total);  
	//drawBoxInRGB(10, 10, 1000, 20, 1.0f, 0.0f, 0.0f);
	return;
}

void checkObject(struct object* obj, bool red){
	//not red = blue ;)

	int xstartl, xstopl, xstep, ystep;
	
	//by default, the object is NOT confirmed.
	obj->confirmed = false;
	
	//low-resolution versions of xstart and xstop
	xstartl = obj->x_start / 64;
	xstopl = obj->x_stop / 64;	
	if(xstopl > (horsumtexture1.Width-1)) xstopl = (horsumtexture1.Width-1); //clamp
	
	//adjust step size to check maximum of 32x8 pixels (skipping some in case of huge objects)
	ystep = (obj->y_stop - obj->y_start)/32;
	if(!ystep) ystep = 1;
	xstep = (xstopl-xstartl)/8;
	if(!xstep) xstep = 1;
	
	
	//for new object dimensions (if applicable)
	int xstartnl = xstartl;
	int xstopnl = xstopl;
	int ystartn = obj->y_start;
	int ystopn = obj->y_stop;
	
	//offset of color inside pixel (red or blue)
	int coloroffset = (red) ? 0 : 2;
	
	bool started = false;
	bool present = false;
	for(int x = xstartl; x<=xstopl; x+=xstep){
		for(int y = obj->y_start; y <= obj->y_stop; y+=ystep){
			if(((char*)horsumtexture1.image)[(horsumtexture1.Width*y+x)*4+coloroffset]){
				obj->confirmed = true; //if we find at least one pixel of the right color, there is an object here.
				present = true;
			}			
		}
		if((!started) && present){
			started = true;
			if(x*64>obj->x_start) obj->x_start = x*64;
		}
		else if(started && (!present)){
			started = false;
			if(x*64<obj->x_stop) obj->x_stop = x*64;
		}
		present = false;
	}	
	started = present = false;
	for(int y = obj->y_start; y <= obj->y_stop; y+=ystep){
		for(int x = xstartl; x<=xstopl; x+=xstep){	
			if(((char*)horsumtexture1.image)[(horsumtexture1.Width*y+x)*4+coloroffset]) present = true;
			//HERE, ROW-BASED METHOD TO MINIMIZE BOX / SPLIT UP IF MULTIPLE OBJECTS.			
		}
		if((!started) && present){
			started = true;
			if(y>obj->y_start) obj->y_start = y;
		}
		else if(started && (!present)){
			started = false;
			if(y<obj->y_stop) obj->y_stop = y;
		}
		present = false;		
	}
}

//-0.830746 0.294118 -0.857612 0.236601
//drawBoxes(render_target, -1.0f, 0.2f, 0.8f, 1.0f );
void drawBoxes(GfxTexture* render_target, float x0i, float y0i, float x1i, float y1i){
	int i;
	float x0,y0,x1,y1;
	for(i=0; i<red_centroid_total_valid; i++){
		if(!object_red_valid[i].confirmed) continue;
		x0 = x0i + (x1i-x0i)*(((float)object_red_valid[i].x_start)/((float)rgbtexture.Width));
		x1 = x0i + (x1i-x0i)*(((float)object_red_valid[i].x_stop)/((float)rgbtexture.Width));
		y0 = y0i + (y1i-y0i)*(((float)object_red_valid[i].y_start)/((float)rgbtexture.Height));
		y1 = y0i + (y1i-y0i)*(((float)object_red_valid[i].y_stop)/((float)rgbtexture.Height));
		//DBG("%f %f %f %f", x0,y0,x1,y1);
		DrawBox(x0,y0,x1,y1,1.0f,0.0f,0.0f, render_target);
		//DrawBox(-0.5,-0.5,0.5,0.5,1.0f,0.0f,1.0f, render_target);
	}
	
	
	
	for(i=0; i<blue_centroid_total_valid; i++){
		if(!object_blue_valid[i].confirmed) continue;
		x0 = x0i + (x1i-x0i)*(((float)object_blue_valid[i].x_start)/((float)rgbtexture.Width));
		x1 = x0i + (x1i-x0i)*(((float)object_blue_valid[i].x_stop)/((float)rgbtexture.Width));
		y0 = y0i + (y1i-y0i)*(((float)object_blue_valid[i].y_start)/((float)rgbtexture.Height));
		y1 = y0i + (y1i-y0i)*(((float)object_blue_valid[i].y_stop)/((float)rgbtexture.Height));
		//DBG("%f %f %f %f", x0,y0,x1,y1);
		DrawBox(x0,y0,x1,y1,0.0f,0.0f,1.0f, render_target);
		//DrawBox(-0.5,-0.5,0.5,0.5,1.0f,0.0f,1.0f, render_target);
	}
	
	//DBG("targetfound %d", targetfound);
	for(i=0; i<targetfound; i++){
		int xstart,xstop,ystart,ystop;
		xstart = target[i].x_start-10;
		xstop = target[i].x_stop +10;
		ystart = target[i].y_start-10;
		ystop = target[i].y_stop +10;
		if(xstart<0) xstart = 0;
		if(xstop>(rgbtexture.Width-1)) xstop = rgbtexture.Width-1;
		if(ystart<0) ystart = 0;
		if(ystop>(rgbtexture.Width-1)) ystop = rgbtexture.Width-1;
		x0 = x0i + (x1i-x0i)*(((float)xstart)/((float)rgbtexture.Width));
		x1 = x0i + (x1i-x0i)*(((float)xstop)/((float)rgbtexture.Width));
		y0 = y0i + (y1i-y0i)*(((float)ystart)/((float)rgbtexture.Height));
		y1 = y0i + (y1i-y0i)*(((float)ystop)/((float)rgbtexture.Height));
		//DBG("%f %f %f %f", x0,y0,x1,y1);
		DrawBox(x0,y0,x1,y1,1.0f,1.0f,0.0f, render_target);
		//DBG("%d %d %d %d", target[i].x_start, target[i].x_stop, target[i].y_start, target[i].y_stop);
	}
	
	//DrawBox(-1.0f,0.2f,0.8f,1.0f,1.0f,1.0f,0.0f, render_target);
	//DrawBox(x0i,y0i,x1i,y1i, 1.0f,1.0f,0.0f,render_target);
	//DrawBox(-0.5f,-0.5f,0.5f,0.5f, 1.0f,1.0f,0.0f,render_target);
}

void drawBoxInRGB(int x0i, int y0i, int x1i, int y1i, float R, float G, float B){
	float x0,y0,x1,y1;
	x0 = -1.0f + 2.0f*(((float)x0i)/((float)rgbtexture.Width));
	x1 = -1.0f + 2.0f*(((float)x1i)/((float)rgbtexture.Width));
	y0 = -1.0f + 2.0f*(((float)y0i)/((float)rgbtexture.Height));
	y1 = -1.0f + 2.0f*(((float)y1i)/((float)rgbtexture.Height));
	DrawBox(x0,y0,x1,y1,R,G,B,&rgbtexture);
}

void showTexWindow(float lowh){
	clear();
	
	mvprintw(0,0,"Transferring and rendering current frame to X window, please wait...");
	
	//finally draw all of this to the screen.
	refresh();
	
	//rectangles defining what to draw where in the window
	SDL_Rect rect = {0,0,g_conf.LOWRES_WIDTH,lowh};
	
	//draw in the window (see Show() functions of textures, which "really" do the work)
	lowdisptexture.Show(&rect);
}

void doInput(void){
	int ch = getch();
	if(ch==ERR){ //no keypress detected? for now set the motor speeds to 0 again.
		struct timespec temp;
		long diffl, diffr, diffu, diffd;
		clock_gettime(CLOCK_REALTIME, &temp);
		diffl = temp.tv_nsec - t_left.tv_nsec;
		diffr = temp.tv_nsec - t_right.tv_nsec;
		diffu = temp.tv_nsec - t_up.tv_nsec;
		diffd = temp.tv_nsec - t_down.tv_nsec;
		if(diffl < 0) diffl += 1000000000;
		if(diffr < 0) diffr += 1000000000;
		if(diffu < 0) diffu += 1000000000;
		if(diffd < 0) diffd += 1000000000;
		if(diffr>60000000 && diffl>60000000 && diffu>60000000 && diffd>60000000){
			doSetSpeed(RIGHT_MOTOR,0);
			doSetSpeed(LEFT_MOTOR, 0);
		}		
	}
	else{ //there was a key detected. check out what we should do.
		while(ch != ERR) //for each key found...
		{
			struct timespec temp;
			long diff;
			switch(ch){ //which key was it?
			case KEY_LEFT:	//left key! if these are coming at high frequency (being held down), turn the motors into a left turn.				
				clock_gettime(CLOCK_REALTIME, &temp);
				diff = temp.tv_nsec - t_left.tv_nsec;
				if(diff < 0) diff += 1000000000;
				t_left = temp;
				if(diff<=60000000){
					doSetSpeedDir(LEFT_MOTOR, BACKWARD, MAX_SPEED);
					doSetSpeedDir(RIGHT_MOTOR,FORWARD, MAX_SPEED);
				}
				break;
			case KEY_UP://up key! if these are coming at high frequency (being held down), turn the motors forward.	
				clock_gettime(CLOCK_REALTIME, &temp);
				diff = temp.tv_nsec - t_up.tv_nsec;
				if(diff < 0) diff += 1000000000;
				t_up = temp;
				if(diff<=60000000){
					doSetSpeedDir(RIGHT_MOTOR,FORWARD, MAX_SPEED);
					doSetSpeedDir(LEFT_MOTOR, FORWARD, MAX_SPEED);
				}
				break;
			case KEY_DOWN://down key! if these are coming at high frequency (being held down), turn the motors backward.	
				clock_gettime(CLOCK_REALTIME, &temp);
				diff = temp.tv_nsec - t_down.tv_nsec;
				if(diff < 0) diff += 1000000000;
				t_down = temp;
				if(diff<=60000000){
					doSetSpeedDir(RIGHT_MOTOR,BACKWARD, MAX_SPEED);
					doSetSpeedDir(LEFT_MOTOR, BACKWARD, MAX_SPEED);
				}
				break;
			case KEY_RIGHT://right key! if these are coming at high frequency (being held down), turn the motors into a right turn.	
				clock_gettime(CLOCK_REALTIME, &temp);
				diff = temp.tv_nsec - t_right.tv_nsec;
				if(diff < 0) diff += 1000000000;
				t_right = temp;
				if(diff<=60000000){
					doSetSpeedDir(RIGHT_MOTOR, BACKWARD, MAX_SPEED);
					doSetSpeedDir(LEFT_MOTOR,FORWARD, MAX_SPEED);
				}
				break;
			case 's': //save framebuffers
				showWindow = true;
				break;
			case 'w': //save framebuffers
				clear();
				mvprintw(0,0, "Downloading and saving texture framebuffers, please wait...");
				refresh();
				//SaveFrameBuffer("tex_fb.png");
				rgbtexture.Save("./captures/tex_rgb.png");
				thresholdtexture.Save("./captures/tex_out.png");
				horsumtexture2.Save("./captures/tex_hor.png");
				versumtexture2.Save("./captures/tex_ver.png");
				break;
			case 'r': //rendering on/off_type
				if(renderScreen) renderScreen = false;
				else renderScreen = true;
				break;
			case 'i': //render from image instead of camera (if we have an image!)
				if(imageAvailable) doImage = !doImage;
				else doImage = false;
				break;
			case 't':
				redadj = (redadj + 1)%4;
				break;
			case 'y':
				redparams[redadj] += 0.1f;
				if(redparams[redadj] > 1.0f) redparams[redadj] -= 1.0f;
				break;
			case 'u':
				redparams[redadj] += 0.01f;
				if(redparams[redadj] > 1.0f) redparams[redadj] -= 1.0f;
				break;
			case 'g':
				blueadj = (blueadj + 1)%4;
				break;
			case 'h':
				blueparams[blueadj] += 0.1f;
				if(blueparams[blueadj] > 1.0f) blueparams[blueadj] -= 1.0f;
				break;
			case 'j':
				blueparams[blueadj] += 0.01f;
				if(blueparams[blueadj] > 1.0f) blueparams[blueadj] -= 1.0f;
				break;
			case 'f':
				if(doFilter) doFilter = false;
				else doFilter = true;
				break;
			case 'b':
				if(doBehave) doBehave = false;
				else doBehave = true;
				break;
			case 'm':
				initFBMap(&lowdisptexture);
				doMap = true;
				DBG("Mapping enabled. Size %dx%d.", lowdisptexture.Width, lowdisptexture.Height);
				break;
			case '1':
				current_Exposure++;
				if(current_Exposure >= NUM_EXPOSURE) current_Exposure = 0;
				raspicamcontrol_set_exposure_mode(cam->CameraComponent, Exposure_Enum[current_Exposure]);
				break;
			case '2':
				current_MeteringMode++;
				if(current_MeteringMode >= NUM_METERINGMODE) current_MeteringMode = 0;
				raspicamcontrol_set_metering_mode(cam->CameraComponent, MeteringMode_Enum[current_MeteringMode]);
				break;
			case '3':
				current_AWB++;
				if(current_AWB >= NUM_AWB) current_AWB = 0;
				raspicamcontrol_set_awb_mode(cam->CameraComponent, AWB_Enum[current_AWB]);
				break;
			case '4':
				current_FX++;
				if(current_FX >= NUM_FX) current_FX = 0;
				//raspicamcontrol_set_fx(cam->CameraComponent, FX_Enum[current_FX]);
				break;
			case 'q': //quit
				endwin();
				exit(1);
			}
	
			move(0,0);
			refresh();
			ch = getch();
		}
	}
}

void doBehaviour(void){
	//first, calculate the angle to the target.
	if(!targetfound){
		//no target
		doSetSpeed(LEFT_MOTOR,0);
		doSetSpeed(RIGHT_MOTOR,0);
		return;
	}
	int angle = (rgbtexture.Width/2)-target[0].c_x; //subtract middle of image from X coordinate to find something proportional to angle from middle.
	angle *= 360;
	angle /= rgbtexture.Width; //normalize to 360 degrees
	
	int absangle = (angle<0) ? -angle : angle;
	
	if(absangle < 10){
		doSetSpeedDir(LEFT_MOTOR, FORWARD, MAX_B_SPEED);
		doSetSpeedDir(RIGHT_MOTOR, FORWARD, MAX_B_SPEED);
	}
	else if(angle>0){
		doSetSpeedDir(LEFT_MOTOR, FORWARD, MAX_B_SPEED);
		doSetSpeedDir(RIGHT_MOTOR, BACKWARD, MAX_B_SPEED);
	}
	else{
		doSetSpeedDir(LEFT_MOTOR, BACKWARD, MAX_B_SPEED);
		doSetSpeedDir(RIGHT_MOTOR, FORWARD, MAX_B_SPEED);
	}
	
	return;
}

void doSetSpeedDir(motor_t motor, direction_t dir, int speed){
	gHaveI2C = setSpeedDir(motor, dir, speed);
	dspeed[(int)motor]=speed;
	ddir[(int)motor]=dir;
	return;
}

void doSetSpeed(motor_t motor, int speed){
	gHaveI2C = setSpeed(motor, speed);
	dspeed[(int)motor]=speed;
	return;
}

void doSetDirection(motor_t motor, direction_t dir){
	gHaveI2C = setDirection(motor, dir);
	ddir[(int)motor]=dir;
	return;
}
