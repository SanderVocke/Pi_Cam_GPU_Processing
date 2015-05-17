/*
VERTICAL SUMMER SHADER

This is almost identical to the horizontal summer shader. Please see that one for comments and explanation.
*/

varying vec2 tcoord;
uniform sampler2D tex;
uniform float step;

void main(void)
{
	//make the sum variable
	float sum = 0.0;
	float pos = tcoord[1];
	//iterate over 64 source pixels using this step, summing their content	
	for(int i=0;i<64;i++){
		sum = sum + texture2D(tex,vec2(tcoord[0],pos)).r;
		pos = pos + step;
	}

	float result = sum/32.0;
	//store the output pixel
	//gl_FragColor = vec4(result,0.0,0.0,1.0);
	gl_FragColor = vec4(result>0.0?1.0:0.0,0.0,0.0,1.0);
}
