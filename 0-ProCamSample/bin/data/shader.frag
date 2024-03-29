#version 150

uniform int useColor;
uniform vec3 color;

uniform int height;
uniform int axis;
uniform int level;
uniform int inverted;
uniform int xcode;
uniform int ycode;

uniform sampler2DRect code;

out vec4 outputColor;

int grayCode(int x) {
    return (x >> 1) ^ x;
}

int isTrue(int x, int i) {
    return (x >> i) & 1;
}

void main() {
    if (useColor == 1) {
        outputColor = vec4(color, 1);
        return;
    }

    int x = int(gl_FragCoord.x) + xcode;
    int y = int(height - gl_FragCoord.y) + ycode; // check this isn't off-by-one
    int src = (axis == 0) ? x : y;

    // old
    // src = grayCode(src);
    // src = isTrue(src, level);

    // new
    src = texture(code, vec2(src, level)).x > 0.5 ? 1 : 0;

    src = inverted == 0 ? src : 1 - src;
    outputColor = vec4(vec3(src),1);
}
