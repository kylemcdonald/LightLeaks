// local

#version 120

#define PI (3.1415926536)
#define TWO_PI (6.2831853072)
#define HALF_PI (PI*0.5)

uniform sampler2DRect xyzMap;
uniform sampler2DRect normalMap;
uniform sampler2DRect confidenceMap;

uniform float elapsedTime;

//Lighthouse
uniform float beamAngle;
uniform float beamWidth;

//Spotlight
uniform float spotlightSize = 0.1;
uniform vec2 spotlightPos = vec2(0.5,0.5);

uniform int stage = 0;


uniform sampler2DRect texture;
uniform vec2 textureSize;

varying vec3 normal;
varying float randomOffset;

uniform vec2 size, mouse;

const bool bw = true;
const bool useStepTime = true;


void main() {
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
	float positionAngle = atan((positionNorm.y - 0.5) / (positionNorm.x-0.5));

	if(stage == 0) {
        //Lighthouse beam

        float angleSpeed = .001;
		float phaseSpeed = .5;
		float scale = 50;
		//b = quasi(time * angleSpeed, (positionNorm) * scale, time * phaseSpeed);
        if(abs(positionAngle - beamAngle+HALF_PI) < beamWidth
           || abs(positionAngle - PI - beamAngle+HALF_PI) < beamWidth
           || abs(positionAngle + PI - beamAngle+HALF_PI) < beamWidth){
            b = 1.;
        } else {
            b = 0.;
        }
		//if(confidence < .01) discard;
	}
    else if(stage == 1){
        //Spotlight
        if( position.z > 0.15 &&
           length(positionNorm.xy - spotlightPos) < spotlightSize){
            b = 1.;
        } else {
            b = 0.;
        }
    }
    else if(stage == 2){
        //Intermezzo
        
    }
    

	// post process
	if(bw) {
		b = b > .5 ? 1 : 0;
	}
    
	gl_FragColor = vec4(vec3(b)/*+position*/, 1.);

}

