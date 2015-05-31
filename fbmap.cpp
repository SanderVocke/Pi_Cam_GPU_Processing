#include "fbmap.h"
#include "graphics.h"
#include "common.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#define FBSIZE (1980*1050*4);

int mmap_fd = -1;
bool initialized = false;
void* mapaddr;
GfxTexture* gMapTex;

void initFBMap(GfxTexture* mapTex){
	if(initialized){
		DBG("Already mapped!");
		return;
	}
	gMapTex = mapTex;
	//create and truncate file if necessary
	system("touch fbmap");
	char trunc_cmd[200];
	sprintf(trunc_cmd, "truncate fbmap -s %lu", (mapTex->Width*mapTex->Height*4));
	system(trunc_cmd);
	
	mmap_fd = open("fbmap", O_RDWR | O_CREAT);	
	mapaddr = mmap(NULL, (size_t)(mapTex->Width*mapTex->Height*4), PROT_READ|PROT_WRITE, MAP_SHARED, mmap_fd, 0);
	if(mapaddr == (void*)-1){
		DBG("Memmap failed! Did you SUDO?");
		exit(1);
	}
	
	DBG("Mapping enabled. Size %dx%d, format RGBA.", mapTex->Width, mapTex->Height);
	
	
	//fill the memmap with green
	int i;
	for(i=0; i<(mapTex->Width*mapTex->Height); i++){
		((unsigned char*)mapaddr)[i*4+0] = 0;
		((unsigned char*)mapaddr)[i*4+1] = 255;
		((unsigned char*)mapaddr)[i*4+2] = 0;
		((unsigned char*)mapaddr)[i*4+3] = 255;
	}	
	initialized = true;
}

void initFBMapVC(VC_RECT_T* rect){
	if(initialized){
		DBG("Already mapped!");
		return;
	}
	//create and truncate file if necessary
	system("touch fbmap");
	char trunc_cmd[200];
	sprintf(trunc_cmd, "truncate fbmap -s %lu", (rect->width*rect->height*2));
	system(trunc_cmd);
	
	mmap_fd = open("fbmap", O_RDWR | O_CREAT);	
	mapaddr = mmap(NULL, (size_t)(rect->width*rect->height*2), PROT_READ|PROT_WRITE, MAP_SHARED, mmap_fd, 0);
	if(mapaddr == (void*)-1){
		DBG("Memmap failed! Did you SUDO?");
		exit(1);
	}
	
	DBG("Mapping enabled. Size %dx%d, format RGB565.", rect->width, rect->height);
		
	//fill the memmap with green
	int i;
	for(i=0; i<(rect->width*rect->height); i++){
		((unsigned char*)mapaddr)[i*2+0] = 255;
		((unsigned char*)mapaddr)[i*2+1] = 255;
	}	
	initialized = true;
}

void* getMapAddr(void){
	return mapaddr;
}