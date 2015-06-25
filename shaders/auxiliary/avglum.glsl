varying vec2 tcoord;
uniform sampler2D tex;
void main(void) 
{
	vec4 color = texture2D(tex, vec2(0.5, 0.5), 11.0);
    float lum = dot(vec3(0.30, 0.59, 0.11), color.xyz);
	gl_FragColor.r = lum;
	gl_FragColor.g = lum;
	gl_FragColor.b = lum;
	gl_FragColor.a = lum;
}
