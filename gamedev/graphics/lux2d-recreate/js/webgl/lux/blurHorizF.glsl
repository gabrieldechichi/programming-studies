precision mediump float;

uniform sampler2D tex_sampler;
uniform int blur; 
varying vec2 tex_coord;
const float blurSize = 1.0/512.0;

void main(void)
{
    if(blur == 0)
    {
        gl_FragColor = texture2D(tex_sampler, tex_coord);
    }
    else
    {
        vec4 sum = vec4(0.0);

        // Horizontal guassian blur with a 9 element kernel
        sum += texture2D(tex_sampler, vec2(tex_coord.x - 4.0*blurSize, tex_coord.y)) * 0.054893;
        sum += texture2D(tex_sampler, vec2(tex_coord.x - 3.0*blurSize, tex_coord.y)) * 0.088240;
        sum += texture2D(tex_sampler, vec2(tex_coord.x - 2.0*blurSize, tex_coord.y)) * 0.123853;
        sum += texture2D(tex_sampler, vec2(tex_coord.x - blurSize, tex_coord.y)) * 0.151793;
        sum += texture2D(tex_sampler, vec2(tex_coord.x, tex_coord.y)) * 0.162443;
        sum += texture2D(tex_sampler, vec2(tex_coord.x + blurSize, tex_coord.y)) * 0.151793;
        sum += texture2D(tex_sampler, vec2(tex_coord.x + 2.0*blurSize, tex_coord.y)) * 0.123853;
        sum += texture2D(tex_sampler, vec2(tex_coord.x + 3.0*blurSize, tex_coord.y)) * 0.088240;
        sum += texture2D(tex_sampler, vec2(tex_coord.x + 4.0*blurSize, tex_coord.y)) * 0.054893;
                            
        gl_FragColor = sum;
    }
}
