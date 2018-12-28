#version 150

uniform mat4 modelViewProjectionMatrix;
in vec4 position;
in vec2 texcoord;
out vec2 texCoordVarying;
out vec4 positionVarying;

void main(){
	texCoordVarying = texcoord;
	positionVarying = position;
    gl_Position = modelViewProjectionMatrix * position;
}
