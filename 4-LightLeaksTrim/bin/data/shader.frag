#version 120

#define PI (3.1415926536)
#define TWO_PI (6.2831853072)
#define HALF_PI (PI*0.5)

uniform sampler2DRect xyzMap;
// uniform sampler2DRect normalMap;
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

const vec3 center = vec3(0.3, .105, 0.05);

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


float stageAlpha(float stageNum, float _stage){
    float d = distance(_stage, stageNum);
    if(d < 1.){
        return 1.-d;
    }
    return 0;
}

//Lighthouse beam
float lighthouse(float elapsedTime,vec3 position,vec3 centered){
    // vec2 rotated = rotate(centered.xy, beamAngle);
    vec2 rotated = rotate(centered.xy, elapsedTime);
    float positionAngle = atan(rotated.y, rotated.x);
    // float positionAngle = elapsedTime;
    
    return 1. - ((positionAngle + PI) / TWO_PI);
}

// Rising strips
float stripes(float time, float position, float size){
    // fast rising stripes
    float b = mod(position * size - time, 1.);
    b *= b;
    return b;
}

float hardStripes(float time, float position, float size){
    // float b = abs(position * size - time);
    // if(b < 0) b = 0.;
    // return b;

    float dist = mod(time - position, size) ;

        if(dist < 0.5 * size){
            return 1.0;
        }
        return 0.0;
}

// float checkerboard(float time,vec3 position){
//     // checkerboard (needs to be animated)
//     vec3 modp = mod(time + position.xyz * 5., 2.);
//     if(modp.x > 1) {
//         return (modp.z < 1 || modp.y > 1) ? 1 : 0;
//     } else {
//         return (modp.z > 1 || modp.y < 1) ? 1 : 0;
//     }
// }

// glittering floor
float glitter(float time, vec3 centered){
    float t = sin(time)*.5;
    vec2 rot = vec2(sin(t), cos(t)) * (1. + sin(time) * .5) + time;
    return sin(50. * dot(rot, centered.xy));
}

// concentric spheres
float circles(float time, vec3 centered, float size){
    return sin(size * mod(length(centered)+(time * 0.05), 10.));
}

float unstableFloor(float time, vec2 position, float size){
    //         // unstable floor
    float t = sin(time)*.25;
    vec2 rot = vec2(sin(t), cos(t));
    return sin(size * dot(rot, position));
}


void main() {
    vec2 projectionOffset = vec2(0);//+vec2(floor( sin(elapsedTime*1)*10) ,0);
    vec3 position = texture2DRect(xyzMap, gl_TexCoord[0].st + projectionOffset).xyz;
    vec3 centered = position - center;
    // vec3 normal = texture2DRect(normalMap, gl_TexCoord[0].st + projectionOffset).xyz;
    float confidence = texture2DRect(confidenceMap, gl_TexCoord[0].st).r;
    if(useConfidence == 0) {
        confidence = 1.;
    }

    // // Position tester:
    // if(position.y > 0.105){
    //   gl_FragColor = vec4(1.,0.,0.,1.);  
    //   return;
    // }


    //gl_FragColor = vec4(vec3(confidence > 0.1 ? 1. : 0.),1.);
    //return;

    if(confidence < 0.2) {
        gl_FragColor = vec4(vec3(0), 1);
        return;
    }

    int numStages = 7;

    float w = 0.;
    vec3 c = vec3(0.,0.,0.);
    

    // Calculate stage
    float t = elapsedTime / 30.;
    float i = mod(t,1.);
    float _stage = t-i;

    // Crossfade
    if(i > 0.9){
        _stage += 1.0 - (1.0 - i) * 10.;
    }

    
    // _stage = 7.; // Overwrite stage
    _stage = mod(_stage,numStages);
    
    int s = 0;

    if(stageAlpha(s, _stage) > 0. && stageAlpha(s, _stage) <= 1.) {
        w += lighthouse(elapsedTime, position, centered) 
        * stageAlpha(s, _stage);    
    }
    
    s++;

    if(stageAlpha(s, _stage) > 0. && stageAlpha(s, _stage) <= 1.) {
        w += stripes(elapsedTime + sin(elapsedTime) 
        * 0.4, position.x, 4) 
        * stageAlpha(s, _stage);
    }

    s++;

    if(stageAlpha(s, _stage) > 0. && stageAlpha(s, _stage) <= 1.) {
        w += glitter(elapsedTime, centered)
        * stageAlpha(s, _stage);
    }

    s++;

    if(stageAlpha(s, _stage) > 0. && stageAlpha(s, _stage) <= 1.) {
        w += circles(sin(elapsedTime * 0.5) * 1.8, centered, 150.) 
        * stageAlpha(s, _stage);
    }

    s++;

    if(stageAlpha(s, _stage) > 0. && stageAlpha(s, _stage) <= 1.) {
        w += unstableFloor(elapsedTime, centered.xz, 100.) 
        * stageAlpha(s, _stage);;
    }

    s++;

    if(stageAlpha(s, _stage) > 0. && stageAlpha(s, _stage) <= 1.) {
        w += hardStripes(elapsedTime * 0.1, position.x, 0.2) 
        * stageAlpha(s, _stage);;
    }

    s++;

    if(stageAlpha(s, _stage) > 0. && stageAlpha(s, _stage) <= 1.) {
        w += hardStripes(elapsedTime * 0.1, position.z + position.y * 0.5, 0.3) 
        * stageAlpha(s, _stage);;
        // c.r = hardStripes(elapsedTime * 0.3, position.z + position.y * 0.5, 0.3);
        // c.b = hardStripes(elapsedTime * 0.2, position.z + position.y * 0.5, 0.3);
    }
    // s++;

    // if(stageAlpha(s, _stage) > 0. && stageAlpha(s, _stage) <= 1.) {
    //     w += checkerboard(elapsedTime * 0.5, position) 
    //     * stageAlpha(s, _stage);
    // }

    // if(_stage >= 0. && _stage < 1.) {
    //     b = lighthouse(elapsedTime, position, centered);
    //     b *= interpolation;
    // }
    // else if(_stage == 1){
    //     //Spotlight
    //     float spotlightDistance = length(position - spotlightPos) / spotlightSize;
    //     b = 0;
    //     if(spotlightDistance < 1) {
    //         b += smoothStep(1. - spotlightDistance);
    //     }
    //     float stripe = sin(elapsedTime * -3. + spotlightDistance * 10.);
    //     if(stripe > .9) {
    //         b += 1.;
    //     }
    //     b = min(b, 1.);
    // } else 
    // if(_stage == 2) {
    //     if(_substage < 1) {
    //         // fast rising stripes
    //         b = mod(position.z * 20. - time * 1, 1.);
    //         b *= b;
    //     } else if(_substage < 2) {
    //         // glittering floor
    //         float t = sin(time)*.5;
    //         vec2 rot = vec2(sin(t), cos(t)) * (1. + sin(time) * .5) + time;
    //         b = sin(50. * dot(rot, centered.xy));
    //     } else if(_substage < 3) {
    //         // concentric spheres
    //         b = sin(200.*mod(length(centered)+(0.02*sin(time*1.)), 10.));
    //     } else if(_substage < 4) {
    //         // unstable floor
    //         float t = sin(time)*.25;
    //         vec2 rot = vec2(sin(t), cos(t));
    //         b = sin(50.*dot(rot, centered.xz));
    //     } else if(_substage < 5)     {
    //         // checkerboard (needs to be animated)
    //         vec3 modp = mod(time + position.xyz * 10., 2.);
    //         if(modp.x > 1) {
    //             b = (modp.z < 1 || modp.y > 1) ? 1 : 0;
    //         } else {
    //             b = (modp.z > 1 || modp.y < 1) ? 1 : 0;
    //         }
    //     } else if(_substage < 6) {
    //         // slower rising stripes
    //         b = mod(position.z - time * 0.5, 1.);
    //         b *= b;
    //     }
    // }
    // else if(_stage == 3){
    //     // Linescan
    //     float scan = mouse.x;

    //     float dist = abs(scan - position[substage]);

    //     if(dist < 0.1){
    //         b = 1.0;
    //     }
    // }


//    b *= smoothStep(stageAmp); // should be more like fast in/out near 0
//    b *= confidence; // for previs
    gl_FragColor = vec4(vec3(w) + c, 1.);
}
