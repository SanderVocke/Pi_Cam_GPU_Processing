#ifndef WINDOW_H
#define WINDOW_H

#include <SDL/SDL.h>

 typedef struct openWindowArgs_t{
	char* image;
	int width;
	int height;
	SDL_Rect *target;
 }openWindowArgs_t;
 
 void updateWindow(void* argptr);

#endif