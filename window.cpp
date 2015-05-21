#include "window.h"
#include "common.h"
#include "graphics.h"
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <SDL/SDL.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
 
static int filter(const SDL_Event * event)
{ return event->type == SDL_QUIT; }

_Bool init_app(const char * name, SDL_Surface * icon, uint32_t flags)
{
    atexit(SDL_Quit);
    if(SDL_Init(flags) < 0)
        return 0;

    SDL_WM_SetCaption(name, name);
    SDL_WM_SetIcon(icon, NULL);

    return 1;
}

void updateWindow(void* argptr){
	static SDL_Surface * screen = NULL;	
	openWindowArgs_t a = *((openWindowArgs_t*)argptr);
	
	if(screen == NULL){
		//SDL part
		uint8_t ok =
			init_app("SDL example", NULL, SDL_INIT_VIDEO) &&
			SDL_SetVideoMode(a.width, a.height, 24, SDL_HWSURFACE);
		assert(ok);
		screen = SDL_GetVideoSurface();
		SDL_SetEventFilter(filter);
	}		

    SDL_Surface * data_sf = SDL_CreateRGBSurfaceFrom(
        a.image, a.width, a.height, 32, a.width * 4,
        0xFF, 0xFF00, 0xFF0000, 0xFF000000);
	
	if(SDL_BlitSurface(data_sf, NULL, screen, a.target) == 0)
		SDL_UpdateRect(screen, 0, 0, 0, 0);
}