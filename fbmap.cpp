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
	initialized = true;
}

void* getMapAddr(void){
	return mapaddr;
}