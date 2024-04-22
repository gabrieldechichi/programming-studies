#version 100

// Input vertex attributes
attribute vec3 vertexPosition;
attribute mat4 instanceTransform;

// Input uniform values
uniform mat4 mvp;

// Output vertex attributes (to fragment shader)
varying vec3 fragPosition;

// NOTE: Add here your custom variables

void main()
{
    mat4 mvpi = mvp*instanceTransform;
    fragPosition = vec3(mvpi*vec4(vertexPosition, 1.0));
    gl_Position = mvpi*vec4(vertexPosition, 1.0);
}
