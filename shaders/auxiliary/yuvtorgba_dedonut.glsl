varying vec2 tcoord;
uniform sampler2D tex0;
uniform sampler2D tex1;
uniform sampler2D tex2;
uniform sampler2D maptex;
void main(void) 
{
	float y = texture2D(tex0,texture2D(maptex,tcoord).rg).r;
	float u = texture2D(tex1,texture2D(maptex,tcoord).rg).r;
	float v = texture2D(tex2,texture2D(maptex,tcoord).rg).r;

	vec4 res;
	res.r = (y + (1.370705 * (v-0.5)));
	res.g = (y - (0.698001 * (v-0.5)) - (0.337633 * (u-0.5)));
	res.b = (y + (1.732446 * (u-0.5)));
	res.a = 1.0;

    gl_FragColor = clamp(res,vec4(0),vec4(1));
}
