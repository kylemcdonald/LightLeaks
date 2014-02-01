#version 120

const float PI = 3.1415926536;
const float TWO_PI = 6.2831853072;

uniform float progress, fade;
uniform float stage;
uniform float positionDivider;
uniform float scrollSpeed;
uniform float offsetSpeed;
uniform float time;
uniform float camoStrength;
uniform float bw;

void main() {
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
