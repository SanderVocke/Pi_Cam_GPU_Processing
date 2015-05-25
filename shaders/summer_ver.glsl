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
	float sum_red = 0.0;
	float sum_blue = 0.0;
	float pos = tcoord[1]-step*31.0;
	vec4 pixel;
	//iterate over 64 source pixels using this step, summing their content	
	for(int i=0;i<64;i++){
		pixel = texture2D(tex,vec2(tcoord[0],pos));	
		sum_red = sum_red + pixel.r;
		sum_blue = sum_blue + pixel.b;
		pos = pos + step;
		
	}
	
	float result_red = sum_red/32.0;
	float result_blue = sum_blue/32.0;
	//store the output pixel
	//gl_FragColor = vec4(result,0.0,0.0,1.0);
	gl_FragColor = vec4(result_red>0.0?1.0:0.0,0.0,result_blue>0.0?1.0:0.0,1.0);
}
