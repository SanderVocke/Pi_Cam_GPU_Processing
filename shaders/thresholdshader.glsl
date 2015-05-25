//VARYING AND UNIFORM VARIABLES: AUTOMATICALLY SET BEFORE STARTING (BY HOST OR VERTEX SHADER)
varying vec2 tcoord; //gives our own coordinate as normalized floating point (x,y)
uniform sampler2D tex; //input texture (panoramic camera image in RGBA)
uniform vec4 redrange; //red thresholding range (min, max, minS, minV). If max<min, we take everything above min and below max (wrap-around).
uniform vec4 bluerange; //same for blue.

vec3 rgb2hsv(vec3 c) //this function converts a vec3 pixel from RGB to HSV. (copied from example code)
{
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) //this function converts a vec3 pixel from HSV to RGB. (copied from example code)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 doThreshold(vec3 hsv) //this function does the thresholding on a RGB pixel, including HSV conversion.
{
    vec3 outThreshold;
	outThreshold	= vec3(0.0,0.0,0.0);
	if(redrange[0] < redrange[1]){
		if((hsv[0] >= redrange[0]) && (hsv[0] <= redrange[1]) && (hsv[1] > redrange[2]) && (hsv[2] > redrange[3])) outThreshold[0] = 1.0;
	}
	else{
		if(((hsv[0] >= redrange[1]) || (hsv[0] <= redrange[0])) && (hsv[1] > redrange[2]) && (hsv[2] > redrange[3])) outThreshold[0] = 1.0;
	}
	
	if(bluerange[0] < bluerange[1]){
		if((hsv[0] >= bluerange[0]) && (hsv[0] <= bluerange[1]) && (hsv[1] > bluerange[2]) && (hsv[2] > bluerange[3])) outThreshold[2] = 1.0;
	}
	else{
		if(((hsv[0] >= bluerange[1]) || (hsv[0] <= bluerange[0])) && (hsv[1] > bluerange[2]) && (hsv[2] > bluerange[3])) outThreshold[2] = 1.0;
	}	
	
	
    //if((hsv[0] >= 0.85) && (hsv[0] <= 1.0) && (hsv[1] > 0.3) && (hsv[2] > 0.3)) outThreshold[0] = 1.0;
	//if((hsv[0] >= 0.40) && (hsv[0] <= 0.85) && (hsv[1] > 0.3) && (hsv[2] > 0.3)) outThreshold[2] = 1.0;
	return outThreshold;
}

vec3 colorize(vec3 c) //unused right now: converts to HSV then back to RGB with maximum S and V ("colorize")
{
    vec3 hsv = rgb2hsv(c);
    return hsv2rgb(vec3(hsv[0], 1, 1));
}

vec3 colorizeSelect(vec3 c) //unused right now: colorize but with a brightness threshold included.
{
    vec3 hsv = rgb2hsv(c);
    if((hsv[1] > 0.15) && (hsv[2] > 0.3)) return hsv2rgb(vec3(hsv[0], 1, 1));
    return vec3(0,0,0);
}

//MAIN FUNCTION
void main(void)
{
    vec3 hsv = rgb2hsv(texture2D( tex, tcoord).rgb);
	gl_FragColor = vec4(doThreshold(hsv), texture2D(tex,tcoord).a);
	//gl_FragColor = vec4(colorizeSelect(texture2D( tex, tcoord).rgb), texture2D(tex,tcoord).a);
}
