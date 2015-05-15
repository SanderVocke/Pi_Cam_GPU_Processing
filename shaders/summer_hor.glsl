/*
HORIZONTAL SUMMER SHADER
This comes after thresholding: its input is the texture which has the color-thresholded version of the input image. its output is a texture that has equal height as the input image, but only 1 pixel in width.
That is because we want to store the sums of each row in a single pixel.

WHAT SHOULD IT DO:
put the sum of each pixel row in a pixel of the output. Note that this is done in a normalized way: output 
between 0.0 and 1.0. This is because that is the only range we can put - 1.0 will become 255 when coming back
to the host. Therefore we normalize by dividing it by the total amount of pixels in the row.

HOW DOES IT WORK NOW:
Pretty straightforward: iterates over its row (remember that the shader is called once for each output pixel, so each shader call will be responsible for doing one row of summing) and stores the sum in RED component.
Right now it also stores a "boolean" value in the BLUE component: if the sum was nonzero, the blue is set to maximum. This is only so that we can easily see it on the preview screen, but serves no "real purpose"!

WHAT NEEDS TO BE DONE:
We will have separate summing of red and blue pixels in the threshold stage later. This should be adapted to also sum them separately.
*/

//VARYING AND UNIFORM VARIABLES: AUTOMATICALLY SET BEFORE STARTING (BY HOST OR VERTEX SHADER)
varying vec2 tcoord;		//gives our own coordinate as normalized floating point (x,y)
uniform sampler2D tex;		//thresholded input texture
uniform float step;

void main(void)
{
	//make the sum variable
	float sum = 0.0;
	float pos = tcoord[0];
	//iterate over 64 source pixels using this step, summing their content	
	for(int i=0;i<64;i++){
		sum = sum + texture2D(tex,vec2(pos,tcoord[1])).r;
		pos = pos + step;
	}

	float result = sum/32.0;
	//store the output pixel
	gl_FragColor = vec4(result,0.0,0.0,1.0);
	//gl_FragColor = vec4(result>0.0?1.0:0.0,0.0,0.0,1.0);
}