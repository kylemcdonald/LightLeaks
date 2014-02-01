#version 120

// danger: taking the sin or cos of elapsedTime will lead to precision errors
// should handle this with roundoff with c++

uniform float elapsedTime;
uniform vec2 size, mouse;
uniform sampler2DRect tex, ao;
uniform float diffuse;
uniform float baseBrightness;

const bool bw = true;
const bool useStepTime = true;
const int positionDivider = 1;
const int stage = 4;

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

float smoothStep(float x) {
	return 3 * pow(x, 2) - 2 * pow(x, 3);
}

// noise
float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main() {
	float b;

	// handle time
	float time = elapsedTime;
	if(useStepTime) {
		time = elapsedTime + sin(elapsedTime);
	}

	// handle space
	vec2 position = vec2(gl_TexCoord[0].x, gl_TexCoord[0].y);
	if(positionDivider > 1) {
		position.x = int(position.x / positionDivider) * positionDivider;
		position.y = int(position.y / positionDivider) * positionDivider;
	}
	vec2 center = size / 2;
	vec2 positionNorm = (position - center) / size.y;
	float positionAngle = atan(positionNorm.y / positionNorm.x);

	if(stage == 0) {
		float angleSpeed = .001;
		float phaseSpeed = .5;
		float scale = 10;
		b = quasi(time * angleSpeed, (positionNorm) * scale, time * phaseSpeed);
	} else if(stage == 1) {
		float chunks = 20;
		float angle = .01 * time;
		b = wave(angle, positionNorm * chunks, time);
	} else if(stage == 2) {
		b = quasi(.0001 * (sin(time) + 1.5) * 400, (positionNorm) * 700, elapsedTime);	
	} else if(stage == 3) {
		float speed = -10;
		float scale = 50;
		b = sin(length(positionNorm) * scale + speed * time);
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
	} else if(stage == 5) {
		b = rand(positionNorm * time);
	}

	// post process
	if(bw) {
		b = b > .5 ? 1 : 0;
	}
	float reflection = texture2DRect(tex, gl_TexCoord[0].st).r;
	float reflected = reflection * b;
	vec2 aoCoord = (gl_TexCoord[0].xy / size) * 512;
	float ambient = .1 * texture2DRect(ao, aoCoord).r;
	gl_FragColor = vec4(vec3(ambient + reflected), 1-(diffuse*.5));
	//gl_FragColor = vec4(vec3(reflected + baseBrightness), diffuse);
}
