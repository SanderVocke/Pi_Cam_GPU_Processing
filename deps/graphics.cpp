#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <iostream>
#include "bcm_host.h"
#include "graphics.h"
#include "window.h"
#include <SDL/SDL.h>
#include "common.h"
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <vc_mem.h>

#include <malloc.h>
#include <stdint.h>
#include <errno.h>

#include "EGL/eglext_brcm.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"
#include <GLES2/gl2.h>
#include <GLES/gl.h>
#include <GLES/glext.h>

#define check() assert(glGetError() == 0)

uint32_t GScreenWidth;
uint32_t GScreenHeight;
EGLDisplay GDisplay;
EGLSurface GSurface;
EGLContext GContext;

GfxShader GDirectVS;
GfxShader GDirectFS;
GfxShader GSimpleVS;
GfxShader GSimpleFS;
GfxShader GYUVFS;
GfxShader GYUVCompFS;
GfxShader GDonutFS;
GfxShader GHorSumFS, GVerSumFS;
GfxShader GCoordFS;
GfxShader GThresholdFS;
GfxShader GErodeFS, GDilateFS;
GfxShader GRangeFS;
GfxProgram GDirectProg;
GfxProgram GSimpleProg;
GfxProgram GYUVProg;
GfxProgram GYUVCompProg;
GfxProgram GDonutProg;
GfxProgram GHorSumProg, GVerSumProg;
GfxProgram GThresholdProg;
GfxProgram GErodeProg, GDilateProg;
GfxProgram GRangeProg;
GLuint GQuadVertexBuffer;
GLuint GLinesVertexBuffer;

DISPMANX_ELEMENT_HANDLE_T dispman_element;
DISPMANX_DISPLAY_HANDLE_T dispman_display;
DISPMANX_RESOURCE_HANDLE_T dispman_resource;
DISPMANX_UPDATE_HANDLE_T dispman_update;
VC_RECT_T dst_rect;
VC_RECT_T src_rect;

uint32_t imgptr;
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (128*1024)

void Finish(){
	glFinish();
}

int doSearch(char* string, int len){
	int fd;
	char *p;
	volatile unsigned char *vc;
	unsigned long address, size, base;
	int i;
	
	fd = open("/dev/vc-mem", O_RDWR | O_SYNC);
	if (fd == -1)	{
		DBG("unable to open /dev/vc-mem ...");
		return 1;
	}
	ioctl(fd, VC_MEM_IOC_MEM_PHYS_ADDR, &address);
	DBG("VC_MEM_IOC_MEM_PHYS_ADDR = %p", (char *)address);
	
	ioctl(fd, VC_MEM_IOC_MEM_SIZE, &size);
	DBG("VC_MEM_IOC_MEM_SIZE = %d", size);
	
	ioctl(fd, VC_MEM_IOC_MEM_BASE, &base);
	DBG("VC_MEM_IOC_MEM_BASE = %p", (char *)base);
	
	vc = (unsigned char *)mmap(	0,	size,	PROT_READ|PROT_WRITE,	MAP_SHARED,	fd,	0);
	if (vc == (unsigned char *)-1)	{
		DBG("mmap failed %s", strerror(errno));
		return -1;
	}
	
	DBG("vc = %p", vc);
	
	DBG("Searching VCMEM...");
	for (i = 0; i < size; i++)	{
		if (strcmp((char *)&vc[i], "Broadcom") == 0)
		{
			DBG("found Broadcom @ %d", i);
			DBG("%2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x\n",
			vc[i], vc[i+1], vc[i+2], vc[i+3], vc[i+4], vc[i+5], vc[i+6], vc[i+7]);
		}
		if (strcmp((char *)&vc[i], "Sander is cool.") == 0)
		{
			DBG("found Sander is cool @ %d", i);
			//DBG("%2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x\n",
			//vc[i], vc[i+1], vc[i+2], vc[i+3], vc[i+4], vc[i+5], vc[i+6], vc[i+7]);
		}
	}
	DBG("Finished searching VCMEM.");
	
	close(fd);
}

void InitGraphics()
{
	bcm_host_init();
	int32_t success = 0;
	EGLBoolean result;
	EGLint num_config;

	static EGL_DISPMANX_WINDOW_T nativewindow;
	
	//first, make sure we are in 1280x720 mode
	//system("tvservice --explicit=\"CEA 4 DVI\"");
	//system("fbset -xres 1280 -yres 720");

	static const EGLint attribute_list[] =
	{
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_NONE
	};

	static const EGLint context_attributes[] = 
	{
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	EGLConfig config;

	// get an EGL display connection
	GDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	assert(GDisplay!=EGL_NO_DISPLAY);
	check();

	// initialize the EGL display connection
	result = eglInitialize(GDisplay, NULL, NULL);
	assert(EGL_FALSE != result);
	check();

	// get an appropriate EGL frame buffer configuration
	result = eglChooseConfig(GDisplay, attribute_list, &config, 1, &num_config);
	assert(EGL_FALSE != result);
	check();

	// get an appropriate EGL frame buffer configuration
	result = eglBindAPI(EGL_OPENGL_ES_API);
	assert(EGL_FALSE != result);
	check();

	// create an EGL rendering context
	GContext = eglCreateContext(GDisplay, config, EGL_NO_CONTEXT, context_attributes);
	assert(GContext!=EGL_NO_CONTEXT);
	check();

	// create an EGL window surface
	success = graphics_get_display_size(0 /* LCD */, &GScreenWidth, &GScreenHeight);
	assert( success >= 0 );

	dst_rect.x = 0;
	dst_rect.y = 0;
	dst_rect.width = GScreenWidth;
	dst_rect.height = GScreenHeight;

	src_rect.x = 0;
	src_rect.y = 0;
	src_rect.width = GScreenWidth << 16;
	src_rect.height = GScreenHeight << 16;

	dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
	dispman_update = vc_dispmanx_update_start( 0 );
	dispman_resource = vc_dispmanx_resource_create (VC_IMAGE_RGB565,GScreenWidth,GScreenHeight,&imgptr);

	dispman_element = vc_dispmanx_element_add ( dispman_update, dispman_display,
		0/*layer*/, &dst_rect, 0,//dispman_resource,//0/*src*/,
		&src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/, (DISPMANX_TRANSFORM_T)0/*transform*/);

	nativewindow.element = dispman_element;
	nativewindow.width = GScreenWidth;
	nativewindow.height = GScreenHeight;
	vc_dispmanx_update_submit_sync( dispman_update );

	check();

	GSurface = eglCreateWindowSurface( GDisplay, config, &nativewindow, NULL );
	assert(GSurface != EGL_NO_SURFACE);
	check();

	// connect the context to the surface
	result = eglMakeCurrent(GDisplay, GSurface, GSurface, GContext);
	assert(EGL_FALSE != result);
	check();
	
	/*
	//write signature
	VC_RECT_T testrect;
	testrect.x = 0;
	testrect.y = 0;
	testrect.width = 32;
	testrect.height = 1;
	char testdata[32] = "Sander is cool.                ";
	//int r = vc_dispmanx_resource_write_data (dispman_resource,VC_IMAGE_RGB565,(GScreenWidth/32+1)*32,testdata,&testrect);
	//DBG("Writedata result: %d", r);
	//find it
	doSearch(NULL, 0);
	*/
	
	// Set background color and clear buffers
	glClearColor(0.15f, 0.25f, 0.35f, 1.0f);
	glClear( GL_COLOR_BUFFER_BIT );

	UpdateShaders();

	//create an ickle vertex buffer
	static const GLfloat quad_vertex_positions[] = {
		0.0f, 0.0f,	1.0f, 1.0f,
		1.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f
	};
	static const GLfloat ver[] = {
		0.0f, 0.0f,	1.0f, 1.0f,
		1.0f, 0.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f
	};	
	glGenBuffers(1, &GQuadVertexBuffer);
	glGenBuffers(1, &GLinesVertexBuffer);
	check();
	glBindBuffer(GL_ARRAY_BUFFER, GQuadVertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertex_positions), quad_vertex_positions, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, GLinesVertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(ver), ver, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	check();
}

void UpdateShaders(void){
	//load the test shaders
	GDirectVS.LoadVertexShader("./shaders/vertex.glsl");
	GSimpleVS.LoadVertexShader("./shaders/auxiliary/vertices.glsl");
	GSimpleFS.LoadFragmentShader("./shaders/auxiliary/copy.glsl");
	GYUVFS.LoadFragmentShader("./shaders/auxiliary/yuvtorgba_dedonut.glsl");
	GHorSumFS.LoadFragmentShader("./shaders/summer_hor.glsl");
	GVerSumFS.LoadFragmentShader("./shaders/summer_ver.glsl");
	GThresholdFS.LoadFragmentShader("./shaders/thresholdshader.glsl");
	GErodeFS.LoadFragmentShader("./shaders/erodefragshader_ours.glsl");
	GDilateFS.LoadFragmentShader("./shaders/dilatefragshader_ours.glsl");
	GDirectFS.LoadFragmentShader("./shaders/color.glsl");
	GRangeFS.LoadFragmentShader("./shaders/showrange.glsl");
	GYUVCompFS.LoadFragmentShader("./shaders/auxiliary/yuvtorgba_dedonut_comp.glsl");
	GDonutFS.LoadFragmentShader("./shaders/auxiliary/yuvtorgba.glsl");
	GSimpleProg.Create(&GSimpleVS,&GSimpleFS);
	GYUVProg.Create(&GSimpleVS,&GYUVFS);
	GHorSumProg.Create(&GSimpleVS,&GHorSumFS);
	GVerSumProg.Create(&GSimpleVS,&GVerSumFS);
	GThresholdProg.Create(&GSimpleVS,&GThresholdFS);
	GErodeProg.Create(&GSimpleVS,&GErodeFS);
	GDilateProg.Create(&GSimpleVS,&GDilateFS);
	GDirectProg.Create(&GDirectVS, &GDirectFS);
	GRangeProg.Create(&GSimpleVS, &GRangeFS);
	GYUVCompProg.Create(&GSimpleVS, &GYUVCompFS);
	GDonutProg.Create(&GSimpleVS, &GDonutFS);
	check();
}

void BeginFrame()
{
	// Prepare viewport
	glViewport ( 0, 0, GScreenWidth, GScreenHeight );
	check();

	// Clear the background
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	check();
}

void EndFrame()
{
	eglSwapBuffers(GDisplay,GSurface);
	check();
}

void ReleaseGraphics()
{

}

// printShaderInfoLog
// From OpenGL Shading Language 3rd Edition, p215-216
// Display (hopefully) useful error messages if shader fails to compile
void printShaderInfoLog(GLint shader)
{
	int infoLogLen = 0;
	int charsWritten = 0;
	GLchar *infoLog;

	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);

	if (infoLogLen > 0)
	{
		infoLog = new GLchar[infoLogLen];
		// error check for fail to allocate memory omitted
		glGetShaderInfoLog(shader, infoLogLen, &charsWritten, infoLog);
		std::cout << "InfoLog : " << std::endl << infoLog << std::endl;
		delete [] infoLog;
	}
}

bool GfxShader::LoadVertexShader(const char* filename)
{
	//cheeky bit of code to read the whole file into memory
	if(Src){
		delete(Src);
		Src = NULL;
	}
	FILE* f = fopen(filename, "rb");
	assert(f);
	fseek(f,0,SEEK_END);
	int sz = ftell(f);
	fseek(f,0,SEEK_SET);
	Src = new GLchar[sz+1];
	fread(Src,1,sz,f);
	Src[sz] = 0; //null terminate it!
	fclose(f);

	//now create and compile the shader
	GlShaderType = GL_VERTEX_SHADER;
	Id = glCreateShader(GlShaderType);
	glShaderSource(Id, 1, (const GLchar**)&Src, 0);
	glCompileShader(Id);
	check();

	//compilation check
	GLint compiled;
	glGetShaderiv(Id, GL_COMPILE_STATUS, &compiled);
	if(compiled==0)
	{
		printf("Failed to compile vertex shader %s:\n%s\n", filename, Src);
		printShaderInfoLog(Id);
		glDeleteShader(Id);
		return false;
	}
	else
	{
		printf("Compiled vertex shader %s:\n%s\n", filename, Src);
	}

	return true;
}

bool GfxShader::LoadFragmentShader(const char* filename)
{
	//cheeky bit of code to read the whole file into memory
	if(Src){
		delete(Src);
		Src = NULL;
	}
	FILE* f = fopen(filename, "rb");
	assert(f);
	fseek(f,0,SEEK_END);
	int sz = ftell(f);
	fseek(f,0,SEEK_SET);
	Src = new GLchar[sz+1];
	fread(Src,1,sz,f);
	Src[sz] = 0; //null terminate it!
	fclose(f);

	//now create and compile the shader
	GlShaderType = GL_FRAGMENT_SHADER;
	Id = glCreateShader(GlShaderType);
	glShaderSource(Id, 1, (const GLchar**)&Src, 0);
	glCompileShader(Id);
	check();

	//compilation check
	GLint compiled;
	glGetShaderiv(Id, GL_COMPILE_STATUS, &compiled);
	if(compiled==0)
	{
		printf("Failed to compile fragment shader %s:\n%s\n", filename, Src);
		printShaderInfoLog(Id);
		glDeleteShader(Id);
		return false;
	}
	else
	{
		printf("Compiled fragment shader %s:\n%s\n", filename, Src);
	}

	return true;
}

bool GfxProgram::Create(GfxShader* vertex_shader, GfxShader* fragment_shader)
{
	VertexShader = vertex_shader;
	FragmentShader = fragment_shader;
	Id = glCreateProgram();
	glAttachShader(Id, VertexShader->GetId());
	glAttachShader(Id, FragmentShader->GetId());
	glLinkProgram(Id);
	check();
	printf("Created program id %d from vs %d and fs %d\n", GetId(), VertexShader->GetId(), FragmentShader->GetId());

	// Prints the information log for a program object
	char log[1024];
	glGetProgramInfoLog(Id,sizeof log,NULL,log);
	printf("%d:program:\n%s\n", Id, log);

	return true;	
}

void DrawRangeRect(float x0, float y0, float x1, float y1,
 float hmin, float hmax, float smin, float vmin,
 GfxTexture* render_target)
{
	if(render_target)
	{
		glBindFramebuffer(GL_FRAMEBUFFER,render_target->GetFramebufferId());
		glViewport ( 0, 0, render_target->GetWidth(), render_target->GetHeight() );
		check();
	}
	
	static bool first = true;

	glUseProgram(GRangeProg.GetId());	check();


	glUniform2f(glGetUniformLocation(GRangeProg.GetId(),"offset"),x0,y0);
	glUniform2f(glGetUniformLocation(GRangeProg.GetId(),"scale"),x1-x0,y1-y0);
	glUniform4f(glGetUniformLocation(GRangeProg.GetId(),"range"),hmin, hmax, smin, vmin);
	check();

	glBindBuffer(GL_ARRAY_BUFFER, GQuadVertexBuffer);	check();

	GLuint loc = glGetAttribLocation(GThresholdProg.GetId(),"vertex");
	glVertexAttribPointer(loc, 4, GL_FLOAT, 0, 16, 0);	check();
	glEnableVertexAttribArray(loc);	check();
	glDrawArrays ( GL_TRIANGLE_STRIP, 0, 4 ); check();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	if(render_target)
	{
		//glFinish();	check();
		//glFlush(); check();
		glBindFramebuffer(GL_FRAMEBUFFER,0);
		glViewport ( 0, 0, GScreenWidth, GScreenHeight );
	}
	
	first = false;
}

void DrawThresholdRect(GfxTexture* texture, float x0, float y0, float x1, float y1,
 float redmin, float redmax, float redsmin, float redvmin,
 float bluemin, float bluemax, float bluesmin, float bluevmin,
 GfxTexture* render_target)
{
	if(render_target)
	{
		glBindFramebuffer(GL_FRAMEBUFFER,render_target->GetFramebufferId());
		glViewport ( 0, 0, render_target->GetWidth(), render_target->GetHeight() );
		check();
	}
	
	static bool first = true;

	glUseProgram(GThresholdProg.GetId());	check();

	if(first){
		glUniform2f(glGetUniformLocation(GThresholdProg.GetId(),"offset"),x0,y0);
		glUniform2f(glGetUniformLocation(GThresholdProg.GetId(),"scale"),x1-x0,y1-y0);
		glUniform1i(glGetUniformLocation(GThresholdProg.GetId(),"tex"), 0);
		check();
	}
	
	glUniform4f(glGetUniformLocation(GThresholdProg.GetId(),"redrange"),redmin, redmax, redsmin, redvmin);
	glUniform4f(glGetUniformLocation(GThresholdProg.GetId(),"bluerange"),bluemin, bluemax, bluesmin, bluevmin);

	glBindBuffer(GL_ARRAY_BUFFER, GQuadVertexBuffer);	check();
	glBindTexture(GL_TEXTURE_2D,texture->GetId());	check();

	GLuint loc = glGetAttribLocation(GThresholdProg.GetId(),"vertex");
	glVertexAttribPointer(loc, 4, GL_FLOAT, 0, 16, 0);	check();
	glEnableVertexAttribArray(loc);	check();
	glDrawArrays ( GL_TRIANGLE_STRIP, 0, 4 ); check();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	if(render_target)
	{
		//glFinish();	check();
		//glFlush(); check();
		glBindFramebuffer(GL_FRAMEBUFFER,0);
		glViewport ( 0, 0, GScreenWidth, GScreenHeight );
	}
	
	first = false;
}

void DrawErode(GfxTexture* texture, float x0, float y0, float x1, float y1, GfxTexture* render_target)
{
	if(render_target)
	{
		glBindFramebuffer(GL_FRAMEBUFFER,render_target->GetFramebufferId());
		glViewport ( 0, 0, render_target->GetWidth(), render_target->GetHeight() );
		check();
	}

	static bool first = true;
	
	glUseProgram(GErodeProg.GetId());	check();

	if(first){
		glUniform2f(glGetUniformLocation(GErodeProg.GetId(),"offset"),x0,y0);
		glUniform2f(glGetUniformLocation(GErodeProg.GetId(),"scale"),x1-x0,y1-y0);
		glUniform1i(glGetUniformLocation(GErodeProg.GetId(),"tex"), 0);
		glUniform2f(glGetUniformLocation(GErodeProg.GetId(),"texelsize"),(1.0f/(float)texture->Width),(1.0f/(float)texture->Height));
		check();
	}

	glBindBuffer(GL_ARRAY_BUFFER, GQuadVertexBuffer);	check();
	glBindTexture(GL_TEXTURE_2D,texture->GetId());	check();

	GLuint loc = glGetAttribLocation(GErodeProg.GetId(),"vertex");
	glVertexAttribPointer(loc, 4, GL_FLOAT, 0, 16, 0);	check();
	glEnableVertexAttribArray(loc);	check();
	glDrawArrays ( GL_TRIANGLE_STRIP, 0, 4 ); check();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	if(render_target)
	{
		//glFinish();	check();
		//glFlush(); check();
		glBindFramebuffer(GL_FRAMEBUFFER,0);
		glViewport ( 0, 0, GScreenWidth, GScreenHeight );
	}
	
	first = false;
}

void DrawDilate(GfxTexture* texture, float x0, float y0, float x1, float y1, GfxTexture* render_target)
{
	if(render_target)
	{
		glBindFramebuffer(GL_FRAMEBUFFER,render_target->GetFramebufferId());
		glViewport ( 0, 0, render_target->GetWidth(), render_target->GetHeight() );
		check();
	}
	
	static bool first = true;

	glUseProgram(GDilateProg.GetId());	check();

	if(first){
		glUniform2f(glGetUniformLocation(GDilateProg.GetId(),"offset"),x0,y0);
		glUniform2f(glGetUniformLocation(GDilateProg.GetId(),"scale"),x1-x0,y1-y0);
		glUniform1i(glGetUniformLocation(GDilateProg.GetId(),"tex"), 0);
		glUniform2f(glGetUniformLocation(GErodeProg.GetId(),"texelsize"),(1.0f/(float)texture->Width),(1.0f/(float)texture->Height));
		check();
	}

	glBindBuffer(GL_ARRAY_BUFFER, GQuadVertexBuffer);	check();
	glBindTexture(GL_TEXTURE_2D,texture->GetId());	check();

	GLuint loc = glGetAttribLocation(GDilateProg.GetId(),"vertex");
	glVertexAttribPointer(loc, 4, GL_FLOAT, 0, 16, 0);	check();
	glEnableVertexAttribArray(loc);	check();
	glDrawArrays ( GL_TRIANGLE_STRIP, 0, 4 ); check();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	if(render_target)
	{
		//glFinish();	check();
		//glFlush(); check();
		glBindFramebuffer(GL_FRAMEBUFFER,0);
		glViewport ( 0, 0, GScreenWidth, GScreenHeight );
	}
	
	first = false;
}

void DrawTextureRect(GfxTexture* texture, float x0, float y0, float x1, float y1, GfxTexture* render_target)
{
	if(render_target)
	{
		glBindFramebuffer(GL_FRAMEBUFFER,render_target->GetFramebufferId());
		glViewport ( 0, 0, render_target->GetWidth(), render_target->GetHeight() );
		check();
	}
	
	static bool first = true;

	glUseProgram(GSimpleProg.GetId());	check();

	glUniform2f(glGetUniformLocation(GSimpleProg.GetId(),"offset"),x0,y0);
	glUniform2f(glGetUniformLocation(GSimpleProg.GetId(),"scale"),x1-x0,y1-y0);
	if(first) glUniform1i(glGetUniformLocation(GSimpleProg.GetId(),"tex"), 0);
	check();

	glBindBuffer(GL_ARRAY_BUFFER, GQuadVertexBuffer);	check();
	glBindTexture(GL_TEXTURE_2D,texture->GetId());	check();

	GLuint loc = glGetAttribLocation(GSimpleProg.GetId(),"vertex");
	glVertexAttribPointer(loc, 4, GL_FLOAT, 0, 16, 0);	check();
	glEnableVertexAttribArray(loc);	check();
	glDrawArrays ( GL_TRIANGLE_STRIP, 0, 4 ); check();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	if(render_target)
	{
		//glFinish();	check();
		//glFlush(); check();
		glBindFramebuffer(GL_FRAMEBUFFER,0);
		glViewport ( 0, 0, GScreenWidth, GScreenHeight );
	}
	
	first = false;
}

void DrawYUVTextureRectComp(GfxTexture* ytexture, GfxTexture* utexture, GfxTexture* vtexture, float x0, float y0, float x1, float y1, GfxTexture* render_target)
{
	if(render_target)
	{
		glBindFramebuffer(GL_FRAMEBUFFER,render_target->GetFramebufferId());
		glViewport ( 0, 0, render_target->GetWidth(), render_target->GetHeight() );
		check();
	}
	
	static bool first = true;

	glUseProgram(GYUVCompProg.GetId());	check();

	if(first){
		glUniform2f(glGetUniformLocation(GYUVCompProg.GetId(),"offset"),x0,y0);
		glUniform2f(glGetUniformLocation(GYUVCompProg.GetId(),"scale"),x1-x0,y1-y0);
		glUniform4f(glGetUniformLocation(GYUVCompProg.GetId(),"donutparams"),
			g_conf.DONUTXRATIO,
			g_conf.DONUTYRATIO,
			g_conf.DONUTINNERRATIO,
			g_conf.DONUTOUTERRATIO);
		glUniform1i(glGetUniformLocation(GYUVCompProg.GetId(),"tex0"), 0);
		glUniform1i(glGetUniformLocation(GYUVCompProg.GetId(),"tex1"), 1);
		glUniform1i(glGetUniformLocation(GYUVCompProg.GetId(),"tex2"), 2);
		check();
	}

	glBindBuffer(GL_ARRAY_BUFFER, GQuadVertexBuffer);	check();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES,ytexture->GetId());	check();
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES,utexture->GetId());	check();
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES,vtexture->GetId());	check();
	glActiveTexture(GL_TEXTURE0);

	GLuint loc = glGetAttribLocation(GYUVCompProg.GetId(),"vertex");
	glVertexAttribPointer(loc, 4, GL_FLOAT, 0, 16, 0);	check();
	glEnableVertexAttribArray(loc);	check();
	glDrawArrays ( GL_TRIANGLE_STRIP, 0, 4 ); check();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	if(render_target)
	{
		//glFinish();	check();
		//glFlush(); check();
		glBindFramebuffer(GL_FRAMEBUFFER,0);
		glViewport ( 0, 0, GScreenWidth, GScreenHeight );
	}
	
	first = false;
}

void DrawYUVTextureRect(GfxTexture* ytexture, GfxTexture* utexture, GfxTexture* vtexture, GfxTexture* maptex, float x0, float y0, float x1, float y1, GfxTexture* render_target)
{
	if(render_target)
	{
		glBindFramebuffer(GL_FRAMEBUFFER,render_target->GetFramebufferId());
		glViewport ( 0, 0, render_target->GetWidth(), render_target->GetHeight() );
		check();
	}
	
	static bool first = true;

	glUseProgram(GYUVProg.GetId());	check();

	if(first){
		glUniform2f(glGetUniformLocation(GYUVProg.GetId(),"offset"),x0,y0);
		glUniform2f(glGetUniformLocation(GYUVProg.GetId(),"scale"),x1-x0,y1-y0);
		glUniform1i(glGetUniformLocation(GYUVProg.GetId(),"tex0"), 0);
		glUniform1i(glGetUniformLocation(GYUVProg.GetId(),"tex1"), 1);
		glUniform1i(glGetUniformLocation(GYUVProg.GetId(),"tex2"), 2);
		glUniform1i(glGetUniformLocation(GYUVProg.GetId(),"maptex"), 3);
		check();
	}

	glBindBuffer(GL_ARRAY_BUFFER, GQuadVertexBuffer);	check();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES,ytexture->GetId());	check();
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES,utexture->GetId());	check();
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES,vtexture->GetId());	check();
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D,maptex->GetId());	check();
	glActiveTexture(GL_TEXTURE0);

	GLuint loc = glGetAttribLocation(GYUVProg.GetId(),"vertex");
	glVertexAttribPointer(loc, 4, GL_FLOAT, 0, 16, 0);	check();
	glEnableVertexAttribArray(loc);	check();
	glDrawArrays ( GL_TRIANGLE_STRIP, 0, 4 ); check();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	if(render_target)
	{
		//glFinish();	check();
		//glFlush(); check();
		glBindFramebuffer(GL_FRAMEBUFFER,0);
		glViewport ( 0, 0, GScreenWidth, GScreenHeight );
	}
	
	first = false;
}

void DrawDonutRect(GfxTexture* ytexture, GfxTexture* utexture, GfxTexture* vtexture, float x0, float y0, float x1, float y1, GfxTexture* render_target)
{
	if(render_target)
	{
		glBindFramebuffer(GL_FRAMEBUFFER,render_target->GetFramebufferId());
		glViewport ( 0, 0, render_target->GetWidth(), render_target->GetHeight() );
		check();
	}
	
	static bool first = true;

	glUseProgram(GDonutProg.GetId());	check();

	if(first){
		glUniform2f(glGetUniformLocation(GDonutProg.GetId(),"offset"),x0,y0);
		glUniform2f(glGetUniformLocation(GDonutProg.GetId(),"scale"),x1-x0,y1-y0);
		glUniform1i(glGetUniformLocation(GDonutProg.GetId(),"tex0"), 0);
		glUniform1i(glGetUniformLocation(GDonutProg.GetId(),"tex1"), 1);
		glUniform1i(glGetUniformLocation(GDonutProg.GetId(),"tex2"), 2);
		check();
	}

	glBindBuffer(GL_ARRAY_BUFFER, GQuadVertexBuffer);	check();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES,ytexture->GetId());	check();
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES,utexture->GetId());	check();
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES,vtexture->GetId());	check();
	glActiveTexture(GL_TEXTURE0);

	GLuint loc = glGetAttribLocation(GYUVProg.GetId(),"vertex");
	glVertexAttribPointer(loc, 4, GL_FLOAT, 0, 16, 0);	check();
	glEnableVertexAttribArray(loc);	check();
	glDrawArrays ( GL_TRIANGLE_STRIP, 0, 4 ); check();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	if(render_target)
	{
		//glFinish();	check();
		//glFlush(); check();
		glBindFramebuffer(GL_FRAMEBUFFER,0);
		glViewport ( 0, 0, GScreenWidth, GScreenHeight );
	}
	
	first = false;
}

void DrawHorSum1(GfxTexture* texture, float x0, float y0, float x1, float y1, GfxTexture* render_target){
	if(render_target)
	{
		glBindFramebuffer(GL_FRAMEBUFFER,render_target->GetFramebufferId());
		glViewport ( 0, 0, render_target->GetWidth(), render_target->GetHeight() );
		check();
	}
	
	static bool first = true;

	glUseProgram(GHorSumProg.GetId());	check();

	if(first){
		glUniform2f(glGetUniformLocation(GHorSumProg.GetId(),"offset"),x0,y0);
		glUniform2f(glGetUniformLocation(GHorSumProg.GetId(),"scale"),x1-x0,y1-y0);
		glUniform1i(glGetUniformLocation(GHorSumProg.GetId(),"tex"), 0);	
		check();
	}
	
	glUniform1f(glGetUniformLocation(GHorSumProg.GetId(),"step"), 1.0f/((float)texture->GetWidth()));

	glBindBuffer(GL_ARRAY_BUFFER, GQuadVertexBuffer);	check();
	glBindTexture(GL_TEXTURE_2D,texture->GetId());	check();

	GLuint loc = glGetAttribLocation(GHorSumProg.GetId(),"vertex");
	glVertexAttribPointer(loc, 4, GL_FLOAT, 0, 16, 0);	check();
	glEnableVertexAttribArray(loc);	check();
	glDrawArrays ( GL_TRIANGLE_STRIP, 0, 4 ); check();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	if(render_target)
	{
		//glFinish();	check();
		//glFlush(); check();
		glBindFramebuffer(GL_FRAMEBUFFER,0);
		glViewport ( 0, 0, GScreenWidth, GScreenHeight );
	}
	
	first = false;
}
void DrawHorSum2(GfxTexture* texture, float x0, float y0, float x1, float y1, GfxTexture* render_target){
	if(render_target)
	{
		glBindFramebuffer(GL_FRAMEBUFFER,render_target->GetFramebufferId());
		glViewport ( 0, 0, render_target->GetWidth(), render_target->GetHeight() );
		check();
	}
	
	static bool first = true;

	glUseProgram(GHorSumProg.GetId());	check();

	if(first){
		glUniform2f(glGetUniformLocation(GHorSumProg.GetId(),"offset"),x0,y0);
		glUniform2f(glGetUniformLocation(GHorSumProg.GetId(),"scale"),x1-x0,y1-y0);
		glUniform1i(glGetUniformLocation(GHorSumProg.GetId(),"tex"), 0);
		check();
	}
	
	glUniform1f(glGetUniformLocation(GHorSumProg.GetId(),"step"), 1.0f/((float)texture->GetWidth()));

	glBindBuffer(GL_ARRAY_BUFFER, GQuadVertexBuffer);	check();
	glBindTexture(GL_TEXTURE_2D,texture->GetId());	check();

	GLuint loc = glGetAttribLocation(GHorSumProg.GetId(),"vertex");
	glVertexAttribPointer(loc, 4, GL_FLOAT, 0, 16, 0);	check();
	glEnableVertexAttribArray(loc);	check();
	glDrawArrays ( GL_TRIANGLE_STRIP, 0, 4 ); check();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	if(render_target)
	{
		//glFinish();	check();
		//glFlush(); check();
		glBindFramebuffer(GL_FRAMEBUFFER,0);
		glViewport ( 0, 0, GScreenWidth, GScreenHeight );
	}
	
	first = false;
}
void DrawVerSum1(GfxTexture* texture, float x0, float y0, float x1, float y1, GfxTexture* render_target){
	if(render_target)
	{
		glBindFramebuffer(GL_FRAMEBUFFER,render_target->GetFramebufferId());
		glViewport ( 0, 0, render_target->GetWidth(), render_target->GetHeight() );
		check();
	}

	static bool first = true;
	
	glUseProgram(GVerSumProg.GetId());	check();

	if(first){
		glUniform2f(glGetUniformLocation(GVerSumProg.GetId(),"offset"),x0,y0);
		glUniform2f(glGetUniformLocation(GVerSumProg.GetId(),"scale"),x1-x0,y1-y0);
		glUniform1i(glGetUniformLocation(GVerSumProg.GetId(),"tex"), 0);
		check();
	}
	
	glUniform1f(glGetUniformLocation(GVerSumProg.GetId(),"step"), 1.0f/((float)texture->GetHeight()));

	glBindBuffer(GL_ARRAY_BUFFER, GQuadVertexBuffer);	check();
	glBindTexture(GL_TEXTURE_2D,texture->GetId());	check();

	GLuint loc = glGetAttribLocation(GVerSumProg.GetId(),"vertex");
	glVertexAttribPointer(loc, 4, GL_FLOAT, 0, 16, 0);	check();
	glEnableVertexAttribArray(loc);	check();
	glDrawArrays ( GL_TRIANGLE_STRIP, 0, 4 ); check();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	if(render_target)
	{
		//glFinish();	check();
		//glFlush(); check();
		glBindFramebuffer(GL_FRAMEBUFFER,0);
		glViewport ( 0, 0, GScreenWidth, GScreenHeight );
	}
	
	first = false;
}
void DrawVerSum2(GfxTexture* texture, float x0, float y0, float x1, float y1, GfxTexture* render_target){
	if(render_target)
	{
		glBindFramebuffer(GL_FRAMEBUFFER,render_target->GetFramebufferId());
		glViewport ( 0, 0, render_target->GetWidth(), render_target->GetHeight() );
		check();
	}
	
	static bool first = true;

	glUseProgram(GVerSumProg.GetId());	check();

	if(first){
		glUniform2f(glGetUniformLocation(GVerSumProg.GetId(),"offset"),x0,y0);
		glUniform2f(glGetUniformLocation(GVerSumProg.GetId(),"scale"),x1-x0,y1-y0);
		glUniform1i(glGetUniformLocation(GVerSumProg.GetId(),"tex"), 0);
		check();
	}
	
	glUniform1f(glGetUniformLocation(GVerSumProg.GetId(),"step"), 1.0f/((float)texture->GetHeight()));

	glBindBuffer(GL_ARRAY_BUFFER, GQuadVertexBuffer);	check();
	glBindTexture(GL_TEXTURE_2D,texture->GetId());	check();

	GLuint loc = glGetAttribLocation(GVerSumProg.GetId(),"vertex");
	glVertexAttribPointer(loc, 4, GL_FLOAT, 0, 16, 0);	check();
	glEnableVertexAttribArray(loc);	check();
	glDrawArrays ( GL_TRIANGLE_STRIP, 0, 4 ); check();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	if(render_target)
	{
		//glFinish();	check();
		//glFlush(); check();
		glBindFramebuffer(GL_FRAMEBUFFER,0);
		glViewport ( 0, 0, GScreenWidth, GScreenHeight );
	}
	
	first = false;
}

void DrawBox(float x0, float y0, float x1,float y1,float R,float G,float B,GfxTexture*render_target){
	if(render_target)
	{
		glBindFramebuffer(GL_FRAMEBUFFER,render_target->GetFramebufferId());
		glViewport ( 0, 0, render_target->GetWidth(), render_target->GetHeight() );
		check();
	}
	
	glBindBuffer(GL_ARRAY_BUFFER, GLinesVertexBuffer);
	glUseProgram(GDirectProg.GetId());
	check();
	
	glUniform2f(glGetUniformLocation(GDirectProg.GetId(),"offset"),x0,y0);
	glUniform2f(glGetUniformLocation(GDirectProg.GetId(),"scale"),x1-x0,y1-y0);
	glUniform4f(glGetUniformLocation(GDirectProg.GetId(),"color"), R, G, B, 1.0f);
	check();
	
	GLuint loc = glGetAttribLocation(GDirectProg.GetId(), "vertex");
	glVertexAttribPointer(loc, 4, GL_FLOAT, 0, 16, 0); check();
	glEnableVertexAttribArray(loc); check();
	glBindBuffer(GL_ARRAY_BUFFER,0);
	check();
	glDrawArrays(GL_LINE_STRIP,0,5);
	if(render_target)
	{
		//glFinish();	check();
		//glFlush(); check();
		glBindFramebuffer(GL_FRAMEBUFFER,0);
		glViewport ( 0, 0, GScreenWidth, GScreenHeight );
	}
}

bool GfxTexture::CreateRGBA(int width, int height, const void* data, GLfloat MinMag, GLfloat Wrap)
{
	Width = width;
	Height = height;
	if(Id) glDeleteTextures(1, &Id);
	glGenTextures(1, &Id);
	check();
	glBindTexture(GL_TEXTURE_2D, Id);
	check();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	check();
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, MinMag);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, MinMag);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, Wrap);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, Wrap);
	check();
	glBindTexture(GL_TEXTURE_2D, 0);
	IsRGBA = true;
	image = NULL;
	return true;
}

bool GfxTexture::CreateGreyScale(int width, int height, const void* data, GLfloat MinMag, GLfloat Wrap)
{
	Width = width;
	Height = height;
	if(Id) glDeleteTextures(1, &Id);
	glGenTextures(1, &Id);
	check();
	glBindTexture(GL_TEXTURE_2D, Id);
	check();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, Width, Height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
	check();
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, MinMag);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, MinMag);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, Wrap);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, Wrap);
	check();
	glBindTexture(GL_TEXTURE_2D, 0);
	IsRGBA = false;
	image = NULL;
	return true;
}

bool GfxTexture::GenerateFrameBuffer()
{
	//Create a frame buffer that points to this texture
	glGenFramebuffers(1,&FramebufferId);
	check();
	glBindFramebuffer(GL_FRAMEBUFFER,FramebufferId);
	check();
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,Id,0);
	check();
	glBindFramebuffer(GL_FRAMEBUFFER,0);
	check();
	return true;
}

void GfxTexture::SetPixels(const void* data)
{
	glBindTexture(GL_TEXTURE_2D, Id);
	check();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, Width, Height, IsRGBA ? GL_RGBA : GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
	check();
	glBindTexture(GL_TEXTURE_2D, 0);
	check();
}

void SaveFrameBuffer(const char* fname)
{
	void* image = malloc(GScreenWidth*GScreenHeight*4);
	glBindFramebuffer(GL_FRAMEBUFFER,0);
	check();
	glReadPixels(0,0,GScreenWidth,GScreenHeight, GL_RGBA, GL_UNSIGNED_BYTE, image);

	unsigned error = lodepng::encode(fname, (const unsigned char*)image, GScreenWidth, GScreenHeight, LCT_RGBA);
	if(error) 
		printf("error: %d\n",error);
}

void GfxTexture::Save(const char* fname)
{
	if(image == NULL) image = malloc(Width*Height*4);
	glBindFramebuffer(GL_FRAMEBUFFER,FramebufferId);
	check();
	glReadPixels(0,0,Width,Height,IsRGBA ? GL_RGBA : GL_LUMINANCE, GL_UNSIGNED_BYTE, image);
	check();
	glBindFramebuffer(GL_FRAMEBUFFER,0);

	unsigned error = lodepng::encode(fname, (const unsigned char*)image, Width, Height, IsRGBA ? LCT_RGBA : LCT_GREY);
	if(error) 
		printf("error: %d\n",error);
}

void GfxTexture::Show(SDL_Rect *target)
{
	if(image == NULL) image = malloc(Width*Height*4);
	glBindFramebuffer(GL_FRAMEBUFFER,FramebufferId);
	check();
	glReadPixels(0,0,Width,Height,IsRGBA ? GL_RGBA : GL_LUMINANCE, GL_UNSIGNED_BYTE, image);
	check();
	glBindFramebuffer(GL_FRAMEBUFFER,0);

	openWindowArgs_t args;
	args.width = Width;
	args.height = Height,
	args.image = (char*)image;
	args.target = target;
	
	updateWindow(&args);
}

void GfxTexture::Get()
{
	if(image == NULL) image = malloc(Width*Height*4);
	glBindFramebuffer(GL_FRAMEBUFFER,FramebufferId);
	check();
	glReadPixels(0,0,Width,Height,IsRGBA ? GL_RGBA : GL_LUMINANCE, GL_UNSIGNED_BYTE, image);
	check();
	glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void GfxTexture::GetTo(void* ptr)
{
	if(ptr == NULL) return;
	glBindFramebuffer(GL_FRAMEBUFFER,FramebufferId);
	check();
	glReadPixels(0,0,Width,Height,IsRGBA ? GL_RGBA : GL_LUMINANCE, GL_UNSIGNED_BYTE, ptr);
	check();
	glBindFramebuffer(GL_FRAMEBUFFER,0);
}
