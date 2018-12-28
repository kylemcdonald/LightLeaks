#version 150

#define PI (3.1415926536)
#define TWO_PI (6.2831853072)
#define HALF_PI (PI*0.5)

uniform sampler2DRect xyzMap;
uniform sampler2DRect confidenceMap;
uniform sampler2DRect mask;
uniform sampler2DRect syphon;
uniform float elapsedTime;
uniform int frameNumber;
uniform vec2 mouse;
uniform vec2 syphonSize;

in vec2 texCoordVarying;
in vec4 positionVarying;
out vec4 outputColor;

// x first axis is along the length of the room, with 0 towards the entrance
// y second axis is along the width of the room, with 0 towards the bar
// z third axis is floor to ceiling, with 0 on the floor
const vec3 center = vec3(0.295, 0.49, 0.06); // balls
const vec3 center_stage = vec3(0.05, 0.50, 0.00); // stage
// 0.25220504 0.47130203 0.05398171

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


float stageAlpha(float stageNum, float stage){
    float d = distance(stage, stageNum);
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

float checkerboard(float time,vec3 position){
    // checkerboard 
    vec3 modp = mod(time + position.xyz * 5., 2.);
    if(modp.x > 1) {
        return (modp.z < 1 || modp.y > 1) ? 1 : 0;
    } else {
        return (modp.z > 1 || modp.y < 1) ? 1 : 0;
    }
}

// glittering floor
float glitter(float time, vec3 centered){
    float t = sin(time)*.5;
    vec2 rot = vec2(sin(t), cos(t)) * (1. + sin(time) * .5) + time;
    return sin(5 * dot(rot, 1+centered.xy));
}

// concentric spheres
float circles(float time, vec3 centered, float size){
    return sin(size * mod(length(centered)+(time * 0.05), 10.));
}

float unstableFloor(float time, vec2 position, float size){
    //         // unstable floor
    float t = sin(time)*.35;
    vec2 rot = vec2(sin(t), cos(t));
    return sin(size * dot(rot, position + vec2(time/ 3000.0))) > 0.8 ? 1.0 : 0.0;
}


void main() {
    vec3 position = texture(xyzMap, texCoordVarying.st).xyz;
    vec3 centered = position - center;
    vec3 centered_stage = position -  center_stage;
    float confidence = texture(confidenceMap, texCoordVarying.st).r;
    float masked = texture(mask, texCoordVarying.st).r;

    /* test white */
    // outputColor = vec4(1);
    // return;

    /* test strobe */
    // float q = .01;
    // if(mod(elapsedTime,q) > (q/2)) {
    //     outputColor = vec4(1);
    // } else {
    //     outputColor = vec4(0,0,0,1);
    // }
    // return;
    
    /* test confidence */
    // outputColor = vec4(vec3(confidence), 1);
    // return;
    
    /* test mask */
    // if(mod(elapsedTime / 2., 1) > 0.5  ){    //     
    //     outputColor = vec4(vec3(masked), 1);
    //     return;
    // }

    /* Discard low confidence and masked */
    if(confidence < 0.08 || masked == 0) {
        outputColor = vec4(vec3(0.), 1.);
        return;
    }

    /* Test full white */
    // outputColor = vec4(1);
    // return;
    
    /* Position tester: */
    // int axis = 1;
    // if(position[axis] > center[axis] + sin(elapsedTime) * 0.005){
    //     outputColor = vec4(1.,0.,0.,1.);
    //     return;
    // } else {
    //     outputColor = vec4(0.,1.,0.,1.);
    //     return;
    // }
    
    /*  Syphon render
        max side of syphon texture = max side of room */
    float syphonScale = max(syphonSize.x, syphonSize.y);
    
    /* use centered.xy it to sample across syphon texture
       this should lay an image on the floor
       change .xy to .yz or .xz to put it on two walls */    
    vec2 syphonCoord = syphonSize/2 + centered.xy * syphonScale;
   
    /* Output syphon */
    // outputColor = texture(syphon, syphonCoord);
    // return; 
    
    
    float w = 0.;
    vec3 c = vec3(0.,0.,0.);
    
    
    // Calculate stage
    float t = elapsedTime / 30.; // duration of each stage
    float stage = floor(t); // index of current stage
    float i = t - stage; // progress in current stage
    
    // Crossfade
    if(i > 0.9){
        stage += 1.0 - (1.0 - i) * 10.;
    }

    int numStages = 9;
    stage = mod(stage, numStages);
    // stage = 8; // Overwrite stage
    
    int s = 0;
    
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        w += lighthouse(elapsedTime, position, centered)
        * stageAlpha(s, stage);
    }
    
    s++;
    
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        w += stripes(elapsedTime + sin(elapsedTime)
                     * 0.4, position.z + position.x * 0.1, 8)
        * stageAlpha(s, stage);
    }
    
    s++;
    
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        w += glitter(elapsedTime, centered)
        * stageAlpha(s, stage);
    }
    
    s++;
    
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        w += circles(sin(0.4 * elapsedTime) * 1.8, centered, 160.)
        * stageAlpha(s, stage);
    }
    
    s++;

   
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        w += unstableFloor(elapsedTime, centered.yx, 150.)
        * stageAlpha(s, stage);;
    }
    
    s++;
   
    
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        w += hardStripes(elapsedTime * 0.10, position.z + position.x * 0.8, 0.1)
        * stageAlpha(s, stage);;
        // c.r = hardStripes(elapsedTime * 0.3, position.z + position.y * 0.5, 0.3);
        // c.b = hardStripes(elapsedTime * 0.2, position.z + position.y * 0.5, 0.3);
    }
    
    s++;
    
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        w += circles(sin(0.4 * elapsedTime) * 1.8, centered_stage , 160.)
        * stageAlpha(s, stage);
    }
    
    s++;
    
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        float ss = sin(elapsedTime/1.0)/2.0;
        float cc = cos(elapsedTime/1.3)/2.0;
        w += hardStripes(elapsedTime * 0.07, position.z + position.x * (ss/ 5.0) + position.y * (cc/ 5.0), 0.15)
        * stageAlpha(s, stage);;
    }
    
    s++;

    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        vec3 p = position - (vec3(cos(elapsedTime*0.5) * 0.10,
                                sin(elapsedTime*0.4) * 0.1,
                                sin(elapsedTime*0.2) * 0.10 ) + center);

        w += (circles(elapsedTime * - 0.5, p, 260.) * stageAlpha(s, stage) > 0.5 ? 1. : 0.);
        
    }
    
    s++;
    // w = hardStripes(elapsedTime * 0.1, position.x, 0.1) ;
    
    outputColor = vec4(vec3(w) + c, 1.);
    
    // avoid a bug that causes some pixels to end up on the far end
    if(position.x == 0){
        outputColor = vec4(0.);
    }
    
    // rgb debug
    // if(texCoordVarying.x < 1920){
    //     outputColor *= vec4(1., 0., 0., 1.0);
    // } else if(texCoordVarying.x < 3840){
    //     outputColor *= vec4(0., 1., 0., 1.0);
    // } else {
    //     outputColor *= vec4(0., 0., 1., 1.0);
    // }

    // if(gl_TexCoord[0].st.y < 1080){
    //      outputColor *= vec4(1.0, 0., 0., 1.0);
    // }
    // if(gl_TexCoord[0].st.y > 1080){
    //      outputColor *= vec4(0.0, 1., 0., 1.0);
    // }
    
    // if(gl_TexCoord[0].st.x < 1920){
    //      outputColor *= vec4(1.0, 0., 0., 1.0);
    // }
    // if(gl_TexCoord[0].st.x > 1920){
    //      outputColor *= vec4(0.0, 1., 0., 1.0);
    // }
    
    // if(gl_TexCoord[0].st.x > 1920){
    // outputColor *= vec4(vec3(0.,0.0,1.0), 1.);
    // return;
    // }
    // if(gl_TexCoord[0].st.y > 1080){
    //     return;
    // }

    // force alpha = 1
    // outputColor.a = 1.;
}

