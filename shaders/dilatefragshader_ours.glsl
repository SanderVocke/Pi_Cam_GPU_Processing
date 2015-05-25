varying vec2 tcoord;
uniform sampler2D tex;
uniform vec2 texelsize;
void main(void) 
{
	vec4 col = vec4(0);
	for(int xoffset = -2; xoffset <= 2; xoffset++)
	{
		for(int yoffset = -2; yoffset <= 2; yoffset++)
		{
			vec2 offset = vec2(xoffset,yoffset);
			col = max(col,texture2D(tex,tcoord+offset*texelsize));
			//col = texture2D(tex,tcoord+offset*texelsize);
			//if(col.r == 1.0) break;
		}
	}
    gl_FragColor = clamp(col,vec4(0),vec4(1));
}
