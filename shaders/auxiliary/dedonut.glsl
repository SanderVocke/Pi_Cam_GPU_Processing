varying vec2 tcoord;
uniform sampler2D tex;
uniform sampler2D maptex;
void main(void) 
{
	gl_FragColor = texture2D(tex,texture2D(maptex,tcoord).rg);
	//gl_FragColor = vec4(1.0,0.0,0.0,1.0);
	//gl_FragColor = texture2D(tex,tcoord);
}
