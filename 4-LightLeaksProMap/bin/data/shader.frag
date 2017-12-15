#version 150

uniform sampler2DRect proMap;
uniform sampler2DRect proConfidence;
uniform float elapsedTime;

in vec2 texCoordVarying;
out vec4 outputColor;

void main() {
    vec3 position = texture(proMap, texCoordVarying.st).xyz;
    float confidence = texture(proConfidence, texCoordVarying.st).r;

    // outputColor = vec4(vec3(confidence), 1);
    // return;

    if(confidence < 0.4) {
        outputColor = vec4(0);
        return;
    }

    // outputColor = vec4(position.xyz * 10, 1);
    float t = (sin(elapsedTime*1) + 1) / 2;
    t *= 0.06;
    if(position.x > t && position.x < t + .01) {
        outputColor = vec4(1);
    } else {
        outputColor = vec4(0);
    }
}
