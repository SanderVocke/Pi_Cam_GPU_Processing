#ifndef __FB_MAP_H__
#define __FB_MAP_H__

#include "fbmap.h"
#include "graphics.h"
#include "common.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void initFBMap(GfxTexture* mapTex);
void* getMapAddr(void);

#endif