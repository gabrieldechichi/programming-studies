attribute vec2 position;
uniform mat4 projection;

varying vec2 screen_pos;

void main(void)
{
    vec4 pos = vec4(position.x*512.0, position.y*512.0, -1.0, 1.0);
    gl_Position = projection * pos;
    screen_pos = pos.xy;
}
