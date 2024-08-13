void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    fragColor = vec4(fragCoord.x / uResolution.x, fragCoord.y / uResolution.y, 0.5, 1.0);
}
