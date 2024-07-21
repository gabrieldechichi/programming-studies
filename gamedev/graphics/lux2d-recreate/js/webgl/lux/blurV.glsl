attribute vec2 position;
uniform mat4 projection;

varying vec2 tex_coord;

void main(void)
{
    gl_Position = projection * vec4(position.x*512.0, position.y*512.0, -1.0, 1.0);
    tex_coord = position;
}
