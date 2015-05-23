varying vec2 tcoord;
uniform sampler2D tex;
uniform vec2 texelsize;
void main(void) 
{
	vec4 col = vec4(1);
	for(int xoffset = -3; xoffset <= 4; xoffset++)
	{
		for(int yoffset = -3; yoffset <= 4; yoffset++)
		{
			vec2 offset = vec2(xoffset,yoffset);
			//col = min(col,texture2D(tex,tcoord+offset*texelsize));
			col = texture2D(tex,tcoord+offset*texelsize);
			if(col.r == 0.0) break;
		}
	}
    gl_FragColor = clamp(col,vec4(0),vec4(1));
}
