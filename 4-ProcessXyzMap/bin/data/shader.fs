// local

#version 120

#define PI (3.1415926536)
#define TWO_PI (6.2831853072)

uniform sampler2DRect xyzMap;
uniform sampler2DRect normalMap;
uniform sampler2DRect confidenceMap;

uniform float elapsedTime;
uniform sampler2DRect texture;
uniform vec2 textureSize;

varying vec3 normal;
varying float randomOffset;

uniform vec2 size, mouse;

const bool bw = true;
const bool useStepTime = true;
const int stage = 1;

// triangle wave from 0 to 1
float wrap(float n) {
	return abs(mod(n, 2.)-1.)*-1. + 1.;
}

// creates a cosine wave in the plane at a given angle
float wave(float angle, vec2 point, float phase) {
	float cth = cos(angle);
	float sth = sin(angle);
	return (cos (phase + cth*point.x + sth*point.y) + 1.) / 2.;
}

const float waves = 20;
// sum cosine waves at various interfering angles
// wrap values when they exceed 1
float quasi(float interferenceAngle, vec2 point, float phase) {
	float sum = 0.;
	for (float i = 0.; i < waves; i++) {
		sum += wave(3.1416*i*interferenceAngle, point, phase);
	}
	return wrap(sum);
}

 float square(float x, float w) {
 	return mod(x, w) > (w / 2) ? 1 : 0;
 }

void main() {
gl_FragColor = vec4(vec3(1), 1);
return;
	vec2 overallOffset = vec2(0);//+vec2(floor( sin(elapsedTime*1)*10) ,0);
	vec4 curSample = texture2DRect(xyzMap, gl_TexCoord[0].st+overallOffset);
	vec4 curSampleNormal = texture2DRect(normalMap, gl_TexCoord[0].st+overallOffset);
	vec4 curSampleConfidence = texture2DRect(confidenceMap, gl_TexCoord[0].st+overallOffset);
	
	vec3 position = curSample.xyz;
	vec3 normalDir = curSampleNormal.xyz;
	float confidence = curSampleConfidence.x;

	float b;

	// handle time
	float time = elapsedTime;
	if(useStepTime) {
		time = elapsedTime + sin(elapsedTime);
	}

	// handle space
	vec2 positionNorm = position.xy;
	float positionAngle = atan(positionNorm.y / positionNorm.x);

	if(stage == 0) {
		float angleSpeed = .001;
		float phaseSpeed = .5;
		float scale = 50;
		b = quasi(time * angleSpeed, (positionNorm) * scale, time * phaseSpeed);
		if(confidence < .01) discard;
	} else if(stage == 1) {
		float chunks = 20;
		float angle = .01 * time;
		b = wave(angle, positionNorm * chunks, time);
		if(confidence < .01) discard;
	} else if(stage == 2) {
		b = quasi(.0001 * (sin(time) + 1.5) * 400, (positionNorm) * 700, elapsedTime);
		if(confidence < .01) discard;
	} else if(stage == 3) {
		float speed = -10;
		float scale = 100;
		b = sin(length(positionNorm) * scale + speed * time);
		if(confidence < .01) discard;
	} else if(stage == 4) {
		float speed = -10;
		float scale = 100;
		float wiggleStrength = .005;
		float petals = 20;
		float spiral = 20;
		float power = .1;
		float wiggle = sin(length(positionNorm) * spiral + time + positionAngle * petals);
		float base = pow(length(positionNorm), power) + wiggle * wiggleStrength;
		b = sin(base * scale + speed * time);	
		if(confidence < .01) discard;
	} else if(stage == 5) {
		b = square(elapsedTime, 100 * sin(elapsedTime));
	}

	// post process
	if(bw) {
		b = b > .5 ? 1 : 0;
	}
	gl_FragColor = vec4(vec3(b), 1.);
}

