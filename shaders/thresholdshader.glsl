/*
THRESHOLD SHADER
This is the first shader in the pipeline here (although on the pi it will be preceded by the one that does YUV->RGB and panoramic-making).
Its input is the RGBA, panoramic input image texture.

WHAT SHOULD IT DO:
It should give the texture which is ready to have the summation techniue applied: the color-thresholded texture. That means RGB->HSV transformation is needed, then some thresholding. But also, if needed, it should do pre-steps such as gaussian blur, erosion, etc to reduce noise. It doesn't do that at the moment.

HOW DOES IT WORK NOW:
remember that it is a shader: it gets called for every single output pixel. Since for this shader the output texture has the same dimensions as the input, it also corresponds to one INPUT pixel.
So all it does at the moment is:
- take its corresponding input pixel
- transform it to HSV
- do thresholding
- make the output pixel (gl_FragColor) white or black depending on the result.

WHAT NEEDS TO BE DONE:
First of all: right now this only thresholds one value (red), and makes the output white or black. We want to distinguish red and blue objects though. So what we could do is make two thresholds. We can store the results of both thresholds in the RED and BLUE components of the output pixel. So if a red object was found, the output pixel will become fully red, if a blue object was found, the output pixel will be fully BLUE.
Second: we need some filters to improve noise performance. You know this better than me! But likely we need gaussian blur and erosion. 
We should avoid re-inventing the wheel. The example code that the PI project was based on is here:
http://robotblogging.blogspot.nl/2013/10/gpu-accelerated-camera-processing-on.html
and the guy had already made GLSL shaders for erosion, gaussian blur and even sobel filtering. Re-use what can be re-used!
It would be great if we can integrate all those steps into this single shader, to avoid having to store even more textures in memory. But if it is impossible for some reason, you can ask me to add another rendering pipeline step(s) before or after this shader to do filtering.
*/

//VARYING AND UNIFORM VARIABLES: AUTOMATICALLY SET BEFORE STARTING (BY HOST OR VERTEX SHADER)
varying vec2 tcoord; //gives our own coordinate as normalized floating point (x,y)
uniform sampler2D tex; //input texture (panoramic camera image in RGBA)

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

vec3 doThreshold(vec3 c) //this function does the thresholding on a RGB pixel, including HSV conversion.
{
    vec3 hsv = rgb2hsv(c);
    if((hsv[0] >= 0.85) && (hsv[0] <= 1.0) && (hsv[1] > 0.2) && (hsv[2] > 0.15)) return vec3(hsv[0], 1, 1);
    return c*vec3(0.0,0.0,0.0);
}

vec3 colorize(vec3 c) //unused right now: converts to HSV then back to RGB with maximum S and V ("colorize")
{
    vec3 hsv = rgb2hsv(c);
    return hsv2rgb(vec3(hsv[0], 1, 1));
}

vec3 colorizeSelect(vec3 c) //unused right now: colorize but with a brightness threshold included.
{
    vec3 hsv = rgb2hsv(c);
    if((hsv[1] > 0.1) && (hsv[2] > 0.1)) return hsv2rgb(vec3(hsv[0], 1, 1));
    return vec3(0,0,0);
}

//MAIN FUNCTION
void main(void)
{
    //gl_FragColor = vec4(doThreshold(texture2D( tex, tcoord).rgb), texture2D(tex,tcoord).a);
	gl_FragColor = vec4(colorizeSelect(texture2D( tex, tcoord).rgb), texture2D(tex,tcoord).a);
}