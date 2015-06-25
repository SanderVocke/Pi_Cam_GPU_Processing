#extension GL_OES_EGL_image_external : require

varying vec2 tcoord;
uniform samplerExternalOES tex;
void main(void) 
{
    gl_FragColor = texture2D(tex,tcoord);
}
