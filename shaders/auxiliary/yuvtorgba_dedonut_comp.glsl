#extension GL_OES_EGL_image_external : require

varying vec2 tcoord;
uniform samplerExternalOES tex0;
uniform samplerExternalOES tex1;
uniform samplerExternalOES tex2;
uniform vec4 donutparams; //(donutcx, donutcy, innerR, outerR)

#define PI (3.141592653589793)
#define TWOPI (6.2831853071795)

void main(void) 
{
	float phi = tcoord.x*TWOPI;
	float d = (1.0-tcoord.y)*(donutparams[3]-donutparams[2])+donutparams[2];
	float dx = d*sin(phi);
	float dy = d*cos(phi);
	float sx = donutparams[0]+dx;
	float sy = donutparams[1]+dy;

	float y = texture2D(tex0,vec2(sx,sy)).r;
	float u = texture2D(tex1,vec2(sx,sy)).r;
	float v = texture2D(tex2,vec2(sx,sy)).r;

	vec4 res;
	res.r = (y + (1.370705 * (v-0.5)));
	res.g = (y - (0.698001 * (v-0.5)) - (0.337633 * (u-0.5)));
	res.b = (y + (1.732446 * (u-0.5)));
	res.a = 1.0;

    gl_FragColor = clamp(res,vec4(0),vec4(1));
}
