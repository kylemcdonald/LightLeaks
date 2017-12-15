#version 120

#define PI (3.1415926536)
#define TWO_PI (6.2831853072)
#define HALF_PI (PI*0.5)

uniform sampler2DRect xyzMap;
uniform sampler2DRect normalMap;
uniform sampler2DRect confidenceMap;
uniform int useConfidence;

uniform float elapsedTime;

//Lighthouse
uniform float beamAngle;
uniform float beamWidth;

//Spotlight
uniform float spotlightSize;
uniform vec3 spotlightPos;

uniform int stage;
uniform int substage;
uniform float stageAmp;

uniform vec2 mouse;

const vec3 center = vec3(0.5, .38, 0.);

vec2 rotate(vec2 position, float amount) {
    mat2 rotation = mat2(vec2( cos(amount),  sin(amount)),
                         vec2(-sin(amount),  cos(amount)));
    return rotation * position;
}

float smoothStep(float x) {
    return 3.*(x*x)-2.*(x*x*x);
}

float sinp(float x) {
    return 1 + sin(x) * .5;
}

void main() {
    vec2 projectionOffset = vec2(0);//+vec2(floor( sin(elapsedTime*1)*10) ,0);
    vec3 position = texture2DRect(xyzMap, gl_TexCoord[0].st + projectionOffset).xyz;
    vec3 centered = position - center;
    vec3 normal = texture2DRect(normalMap, gl_TexCoord[0].st + projectionOffset).xyz;
    float confidence = texture2DRect(confidenceMap, gl_TexCoord[0].st).r;
    if(useConfidence == 0) {
        confidence = 1.;
    }

    //gl_FragColor = vec4(vec3(confidence > 0.1 ? 1. : 0.),1.);
    //return;

    if(confidence < 0.1) {
        gl_FragColor = vec4(vec3(0), 1);
        return;
    }

    float b = 0.;
    // int _stage = stage;
    int _stage = 2;
    int _substage = 0;

    // handle time
    float time = elapsedTime;
    time = elapsedTime + sin(elapsedTime); // step time
    // handle space
    if(_stage == 0) {
        //Lighthouse beam
        vec2 rotated = rotate(centered.xy, beamAngle);
        float positionAngle = atan(rotated.y, rotated.x);
        b = 1. - ((positionAngle + PI) / TWO_PI);
    }
    else if(_stage == 1){
        //Spotlight
        float spotlightDistance = length(position - spotlightPos) / spotlightSize;
        b = 0;
        if(spotlightDistance < 1) {
            b += smoothStep(1. - spotlightDistance);
        }
        float stripe = sin(elapsedTime * -3. + spotlightDistance * 10.);
        if(stripe > .9) {
            b += 1.;
        }
        b = min(b, 1.);
    } else 
    if(_stage == 2) {
        if(_substage < 1) {
            // fast rising stripes
            b = mod(position.z * 20. - time * 1, 1.);
            b *= b;
        } else if(_substage < 2) {
            // glittering floor
            float t = sin(time)*.5;
            vec2 rot = vec2(sin(t), cos(t)) * (1. + sin(time) * .5) + time;
            b = sin(50. * dot(rot, centered.xy));
        } else if(_substage < 3) {
            // concentric spheres
            b = sin(200.*mod(length(centered)+(0.02*sin(time*1.)), 10.));
        } else if(_substage < 4) {
            // unstable floor
            float t = sin(time)*.25;
            vec2 rot = vec2(sin(t), cos(t));
            b = sin(50.*dot(rot, centered.xz));
        } else if(_substage < 5)     {
            // checkerboard (needs to be animated)
            vec3 modp = mod(time + position.xyz * 10., 2.);
            if(modp.x > 1) {
                b = (modp.z < 1 || modp.y > 1) ? 1 : 0;
            } else {
                b = (modp.z > 1 || modp.y < 1) ? 1 : 0;
            }
        } else if(_substage < 6) {
            // slower rising stripes
            b = mod(position.z - time * 0.5, 1.);
            b *= b;
        }
    }
    else if(_stage == 3){
        // Linescan
        float scan = mouse.x;

        float dist = abs(scan - position[substage]);

        if(dist < 0.1){
            b = 1.0;
        }
    }


//    b *= smoothStep(stageAmp); // should be more like fast in/out near 0
//    b *= confidence; // for previs
    gl_FragColor = vec4(vec3(b), 1.);
}
