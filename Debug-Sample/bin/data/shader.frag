#version 150

uniform int height;
uniform int axis;
uniform int level;
uniform int inverted;
uniform int xcode;
uniform int ycode;

out vec4 outputColor;

int grayCode(int x) {
	return (x >> 1) ^ x;
}

int isTrue(int x, int i) {
	return (x >> i) & 1;
}

void main() {
    int x = int(gl_FragCoord.x) + xcode;
	int y = int(height - gl_FragCoord.y) + ycode; // check this isn't off-by-one
	int src = (axis == 0) ? x : y;
	src = grayCode(src);
	src = isTrue(src, level);
    src = inverted == 0 ? src : 1 - src;
	outputColor = vec4(vec3(src),1);
}
