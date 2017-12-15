#version 120

#define PI (3.1415926536)
#define TWO_PI (6.2831853072)
#define HALF_PI (PI*0.5)

uniform sampler2DRect xyzMap;
uniform sampler2DRect confidenceMap;
uniform sampler2DRect calibrationMap;
uniform float elapsedTime;
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
    vec2 projectionPosition = gl_TexCoord[0].st;
    
    if(gl_TexCoord[0].st.x < 1920 && gl_TexCoord[0].st.y < 1080){
        // red
        projectionOffset += vec2(0.,-10.0);
    } else if(gl_TexCoord[0].st.x > 1920 && gl_TexCoord[0].st.y < 1080){
        // green
        projectionOffset += vec2(0.,-10.0);
    } else if(gl_TexCoord[0].st.x < 1920 && gl_TexCoord[0].st.y > 1080){
        // blue
        projectionOffset += vec2(0.,-10.0);
    } else if(gl_TexCoord[0].st.x > 1920 && gl_TexCoord[0].st.y > 1080){
        // yellow
        projectionOffset += vec2(0.,-10.0);
    }

    vec3 position = texture2DRect(xyzMap, projectionPosition + projectionOffset).xyz;
    float calibration = 255 * texture2DRect(calibrationMap, gl_TexCoord[0].st + projectionOffset).x;
    vec3 centered = position - center;
    // vec3 normal = texture2DRect(normalMap, gl_TexCoord[0].st + projectionOffset).xyz;
    float confidence = texture2DRect(confidenceMap, gl_TexCoord[0].st).r;

    if(confidence < 0.2) {
        gl_FragColor = vec4(vec3(0.), 1.);
        return;
    }

//      gl_FragColor = vec4(vec3(1.), 1.);
// return;
    // if(calibration != 1. ){
    //     return;
    // }

    // if(gl_TexCoord[0].st.x > 1920){
    //     // gl_FragColor *= vec4(vec3(0.,1.0,0.0), 1.);
    //     return;
    // }
    // if(gl_TexCoord[0].st.y > 1080){
    //     return;
    // }
    // // Position tester:
    // if(position.x > center.x + sin(elapsedTime) * .03){
    // // if(position.x > center.x - 0.3){
    //   gl_FragColor = vec4(1.,0.,0.,1.);  
    //   return;
    // } else {
    //     gl_FragColor = vec4(0.,1.,0.,1.);  
    //   return;
    // }


    //gl_FragColor = vec4(vec3(confidence > 0.1 ? 1. : 0.),1.);
    //return;


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

    
    _stage = 5.; // Overwrite stage
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

    // w = hardStripes(elapsedTime * 0.1, position.x, 0.1) ;
        
    gl_FragColor = vec4(vec3(w) + c, 1.);

    if(position.x == 0){
        // gl_FragColor = vec4(0.);
    }

    // if(gl_TexCoord[0].st.x < 1920 && gl_TexCoord[0].st.y < 1080){
    //     gl_FragColor *= vec4(1.0, 0.2, 0., 0.0);
    // } else if(gl_TexCoord[0].st.x > 1920 && gl_TexCoord[0].st.y < 1080){
    //     gl_FragColor *= vec4(0.0, 1., 0., 1.0);
    // } else if(gl_TexCoord[0].st.x < 1920 && gl_TexCoord[0].st.y > 1080){
    //     gl_FragColor *= vec4(0.0, 0.4, 1., 0.0);
    // } else if(gl_TexCoord[0].st.x > 1920 && gl_TexCoord[0].st.y > 1080){
    //     gl_FragColor *= vec4(0.5, 0.5, 0., 0.0);
    // }
    // if(gl_TexCoord[0].st.y < 1080){
    //      gl_FragColor *= vec4(1.0, 0., 0., 1.0);
    // }
    // if(gl_TexCoord[0].st.y > 1080){
    //      gl_FragColor *= vec4(0.0, 1., 0., 1.0);
    // }

    // if(gl_TexCoord[0].st.x < 1920){
    //      gl_FragColor *= vec4(1.0, 0., 0., 1.0);
    // }
    // if(gl_TexCoord[0].st.x > 1920){
    //      gl_FragColor *= vec4(0.0, 1., 0., 1.0);
    // }

    // if(gl_TexCoord[0].st.x > 1920){
        // gl_FragColor *= vec4(vec3(0.,0.0,1.0), 1.);
        // return;
    // }
    // if(gl_TexCoord[0].st.y > 1080){
    //     return;
    // }
}
