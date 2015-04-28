#include "graphics.h"
#include "camera.h"
#include "common.h"
#include <stdio.h>


int main(int argc, const char **argv)
{
	//init graphics and camera
	DBG("Start.");
	DBG("Initializing graphics and camera.");
	CCamera* cam = StartCamera(CAPTURE_WIDTH, CAPTURE_HEIGHT, 10, 1, false); //10fps camera
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
	yreadtexture.CreateRGBA(MAIN_TEXTURE_WIDTH,MAIN_TEXTURE_HEIGHT);
	yreadtexture.GenerateFrameBuffer();
	ureadtexture.CreateRGBA(MAIN_TEXTURE_WIDTH/2,MAIN_TEXTURE_HEIGHT/2);
	ureadtexture.GenerateFrameBuffer();
	vreadtexture.CreateRGBA(MAIN_TEXTURE_WIDTH/2,MAIN_TEXTURE_HEIGHT/2);
	vreadtexture.GenerateFrameBuffer();
	
	//Start the processing loop.
	DBG("Starting process loop.");
	
	while(1){}
	
	return 0;
}