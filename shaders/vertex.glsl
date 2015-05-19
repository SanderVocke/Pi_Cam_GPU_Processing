attribute vec4 vertex;
uniform vec2 offset;
uniform vec2 scale;
void main(void) 
{
	vec4 pos = vertex;
	pos.xy = pos.xy*scale+offset;
	gl_Position = pos;
}