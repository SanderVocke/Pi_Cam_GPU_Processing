varying vec2 tcoord;
uniform vec4 range;

vec3 hsv2rgb(vec3 c) //this function converts a vec3 pixel from HSV to RGB. (copied from example code)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main(void)
{
	float h, s, v;
	
	s = v = 1.0;
	if(tcoord.y < 0.5){
		s = range[2] + (1.0-range[2])*(tcoord.y*2.0);
	}
	else{
		v = 1.0 - (1.0-range[3])*((tcoord.y-0.5)*2.0);
	}
	
	if(range[1]>range[0]) h = range[0] + (range[1]-range[0])*tcoord.x;
	else{
		h = range[0] + (range[1]+1.0-range[0])*tcoord.x;
		if(h>1.0) h = h-1.0;
	}	
	
	gl_FragColor = vec4(hsv2rgb(vec3(h,s,v)),1.0);
}
