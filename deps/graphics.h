#pragma once

#include "GLES2/gl2.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "lodepng.h"
#include <SDL/SDL.h>

void InitGraphics();
void ReleaseGraphics();
void BeginFrame();
void EndFrame();
void Finish();


class GfxShader
{
	GLchar* Src;
	GLuint Id;
	GLuint GlShaderType;

public:

	GfxShader() : Src(NULL), Id(0), GlShaderType(0) {}
	~GfxShader() { if(Src) delete[] Src; }

	bool LoadVertexShader(const char* filename);
	bool LoadFragmentShader(const char* filename);
	GLuint GetId() { return Id; }
};

class GfxProgram
{
	GfxShader* VertexShader;
	GfxShader* FragmentShader;
	GLuint Id;

public:

	GfxProgram() {}
	~GfxProgram() {}

	bool Create(GfxShader* vertex_shader, GfxShader* fragment_shader);
	GLuint GetId() { return Id; }
};

class GfxTexture
{
	GLuint Id;
	bool IsRGBA;

	GLuint FramebufferId;
public:
	void* image;
	int Width,Height;

	GfxTexture() : Width(0), Height(0), Id(0), FramebufferId(0) {}
	~GfxTexture() {}

	bool CreateRGBA(int width, int height, const void* data = NULL, GLfloat MinMag=GL_NEAREST, GLfloat Wrap=GL_CLAMP_TO_EDGE);
	bool CreateGreyScale(int width, int height, const void* data = NULL, GLfloat MinMag=GL_NEAREST, GLfloat Wrap=GL_CLAMP_TO_EDGE);
	bool GenerateFrameBuffer();
	void SetPixels(const void* data);
	GLuint GetId() { return Id; }
	GLuint GetFramebufferId() { return FramebufferId; }
	int GetWidth() {return Width;}
	int GetHeight() {return Height;}
	void Save(const char* fname);
	void Show(SDL_Rect* target);
	void Get();
};

void SaveFrameBuffer(const char* fname);

void DrawThresholdRect(GfxTexture* texture, float x0, float y0, float x1, float y1, GfxTexture* render_target);
void DrawTextureRect(GfxTexture* texture, float x0, float y0, float x1, float y1, GfxTexture* render_target);
void DrawYUVTextureRect(GfxTexture* ytexture, GfxTexture* utexture, GfxTexture* vtexture, GfxTexture* maptex, float x0, float y0, float x1, float y1, GfxTexture* render_target);
void DrawDeDonutTextureRect(GfxTexture* texture, GfxTexture* dedonutmap, float x0, float y0, float x1, float y1, GfxTexture* render_target);
void DrawHorSum1(GfxTexture* texture, float x0, float y0, float x1, float y1, GfxTexture* render_target);
void DrawHorSum2(GfxTexture* texture, float x0, float y0, float x1, float y1, GfxTexture* render_target);
void DrawVerSum1(GfxTexture* texture, float x0, float y0, float x1, float y1, GfxTexture* render_target);
void DrawVerSum2(GfxTexture* texture, float x0, float y0, float x1, float y1, GfxTexture* render_target);
void DrawCoordinates(GfxTexture* hortexture, GfxTexture* vertexture, float x0, float y0, float x1, float y1, float numcoords, GfxTexture* render_target);
void DrawBox(float x0, float y0, float x1,float y1,float R,float G,float B,GfxTexture* render_target);

void UpdateShaders(void);
