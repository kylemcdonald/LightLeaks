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
uniform int audio;
uniform int debugMode;

in vec2 texCoordVarying;
in vec4 positionVarying;
out vec4 outputColor;

// SETTINGS
int numProjectors = 3;
float threshold = 0.1; //26;
// x first axis is along the length of the room
// y second axis is floor to ceiling, with 0 on the floor
// z third axis is along the width of the room
const vec3 center = vec3(0.322, 0.220, 0.323); // balls
const vec3 center_alt = vec3(0.000, 0.000, 0.000); // stage
int numStages = 17;

// #define TEST_POSITION 2
// #define TEST_POSITION_ALT 1
// #define OVERWRITE_STAGE 0
// #define MOUSE_BEAM

#define M_PI 3.14159265358979323846

float rand(vec2 co){return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);}
float rand (vec2 co, float l) {return rand(vec2(rand(co), l));}
float rand (vec2 co, float l, float t) {return rand(vec2(rand(co, l), t));}

float perlin(vec2 p, float dim, float time) {
	vec2 pos = floor(p * dim);
	vec2 posx = pos + vec2(1.0, 0.0);
	vec2 posy = pos + vec2(0.0, 1.0);
	vec2 posxy = pos + vec2(1.0);
	
	float c = rand(pos, dim, time);
	float cx = rand(posx, dim, time);
	float cy = rand(posy, dim, time);
	float cxy = rand(posxy, dim, time);
	
	vec2 d = fract(p * dim);
	d = -0.5 * cos(d * M_PI) + 0.5;
	
	float ccx = mix(c, cx, d.x);
	float cycxy = mix(cy, cxy, d.x);
	float center = mix(ccx, cycxy, d.y);
	
	return center * 2.0 - 1.0;
}

// p must be normalized!
float perlin(vec2 p, float dim) {
	return perlin(p, dim, 0.0);
}

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
    return (d < 1.) ? 1. - d : 0;
}

//Lighthouse beam
float lighthouse(float t, vec3 position,vec3 centered){
    // vec2 rotated = rotate(centered.xy, beamAngle);
    vec2 rotated = rotate(centered.xz, t);
    float positionAngle = atan(rotated.y, rotated.x);
    // float positionAngle = t;
    
    return 1. - ((positionAngle + PI) / TWO_PI);
}

// Rising strips
float stripes(float time, float position, float size){
    // fast rising stripes
    float b = mod(position * size - time, 1.);
    b *= b;
    return b;
}

float hardStripes(float time, float position, float size, float width){
    // float b = abs(position * size - time);
    // if(b < 0) b = 0.;
    // return b;
    
    float dist = mod(time - position, size) ;
    
    if(dist < width * size){
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
    float glitterFrequency = 4;
    float glitterSpeed = 1;
    float t = sin(time) * 0.5;
    vec2 rot = vec2(sin(t), cos(t)) * (1. + sin(time) * .5) + time;
    return sin(glitterFrequency * dot(rot, glitterSpeed - centered.xy));
}

// concentric spheres
float circles(float time, vec3 centered, float size){
    return sin(size * mod(length(centered)+(time * 0.05), 10.));
}

float unstableFloor(float time, vec2 position, float size){
    //         // unstable floor
    float t = sin(time)*.15;
    vec2 rot = vec2(sin(t), cos(t));
    return smoothstep(0.8, 0.9, sin(size * dot(rot, position + vec2(sin(time) / 10.0))));
}

float radialLine(vec2 position, vec2 centered, float r, float width){
    vec2 rotated = rotate(centered.xy, r);
    float positionAngle = atan(rotated.y, rotated.x);
    // float positionAngle = t;
    return positionAngle > width ? 1 : 0;
    // return  ((positionAngle + PI) / TWO_PI);
}

int getProjectorNum(){
    float x = gl_FragCoord.x;
    float w = textureSize(confidenceMap).x;
    float p = numProjectors * x / w;
    return int(p);
}

vec2 getProjectorSize(){
    float w = textureSize(confidenceMap).x / numProjectors;
    float h = textureSize(confidenceMap).y;
    return vec2(w,h);
}

vec2 getProjectorOffsetCoordinate(vec2 texCoord){
    vec2 ret = texCoord;
    ret.x -= getProjectorSize().x * getProjectorNum();
    return ret;
}

vec2 adjustOffset(vec2 texCoord){
    int p = getProjectorNum();
    
    if(p == 0){
        // texCoord += vec2(100, 10);
    }
    if(p == 1){
        // texCoord += vec2(3, 1);
    }
    if(p == 2){
        // texCoord += vec2(2, 0);
    }
    if(p == 3){
        // texCoord += vec2(0, -1);
    }
    // position.y += p;
    return texCoord;
}

vec3 calculateBeamVector(vec2 texCoord){
    vec3 hit_area_tl = vec3(0.0332,0.4287,0.0888);
    vec3 hit_area_tr = vec3(0.0332,0.5723,0.0888);
    vec3 hit_area_bl = vec3(0.332,0.4287,0.0282);

    vec2 texCoordProjectorSpace = getProjectorOffsetCoordinate(texCoord) / getProjectorSize();
    vec3 hit_point = hit_area_tl;
    hit_point += (hit_area_tr - hit_area_tl) * texCoordProjectorSpace.x;
    hit_point += (hit_area_bl - hit_area_tl) * texCoordProjectorSpace.y;

    return texture(xyzMap, texCoord.st).xyz - hit_point;
}

vec2 getProjectorFragCoord() {
    vec2 projectorSize = getProjectorSize();
    float x = mod(gl_FragCoord.x, projectorSize.x);
    float y = mod(gl_FragCoord.y, projectorSize.y);
    return vec2(x, y);
}

vec2 random2( vec2 p ) {
    return fract(sin(vec2(dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3))))*43758.5453);
}

vec3 voronoi( in vec2 x, float rnd, float t ) {
    vec2 n = floor(x);
    vec2 f = fract(x);

    // first pass: regular voronoi
    vec2 mg, mr;
    float md = 8.0;
    for (int j=-1; j<=1; j++ ) {
        for (int i=-1; i<=1; i++ ) {
            vec2 g = vec2(float(i),float(j));
            vec2 o = random2( n + g )*rnd;
            // #ifdef ANIMATE
            o = 0.5 + 0.5*sin( t + 6.2831*o );
            // #endif
            vec2 r = g + o - f;
            float d = dot(r,r);

            if( d<md ) {
                md = d;
                mr = r;
                mg = g;
            }
        }
    }

    // second pass: distance to borders
    md = 8.0;
    for (int j=-2; j<=2; j++ ) {
        for (int i=-2; i<=2; i++ ) {
            vec2 g = mg + vec2(float(i),float(j));
            vec2 o = random2(n + g)*rnd;
            // #ifdef ANIMATE
            o = 0.5 + 0.5*sin( t + 6.2831*o );
            // #endif
            vec2 r = g + o - f;

            if( dot(mr-r,mr-r)>0.00001 )
            md = min( md, dot( 0.5*(mr+r), normalize(r-mr) ) );
        }
    }
    return vec3( md, mr );
}

// vec2 movingTiles(vec2 _st, float _zoom, float _speed, float t){
//     _st *= _zoom;
//     float time = t*_speed;
//     if( fract(time)>0.5 ){
//         if (fract( _st.y * 0.5) > 0.5){
//             _st.x += fract(time)*2.0;
//         } else {
//             _st.x -= fract(time)*2.0;
//         }
//     } else {
//         if (fract( _st.x * 0.5) > 0.5){
//             _st.y += fract(time)*2.0;
//         } else {
//             _st.y -= fract(time)*2.0;
//         }
//     }
//     return fract(_st);
// }

// float circle(vec2 _st, float _radius){
//     vec2 pos = vec2(0.5)-_st;
//     return smoothstep(1.0-_radius,1.0-_radius+_radius*0.2,1.-dot(pos,pos)*3.14);
// }


void main() {
    float elapsedTimeMod = elapsedTime;
    if (debugMode == 13) { 
        elapsedTimeMod = 15 + 30 * floor(elapsedTime/5);
    }

    vec2 texCoord = adjustOffset(texCoordVarying.st);

    vec3 position = texture(xyzMap, texCoord.st).xyz;    

    vec3 centered = position - center;
    vec3 centered_stage = position -  center_alt;
    float confidence = texture(confidenceMap, texCoord.st).r;
    float masked = texture(mask, texCoord.st).r;

    // Calculate stage
    float t = elapsedTimeMod / 30.; // duration of each stage
    float stage = floor(t); // index of current stage
    float i = t - stage; // progress in current stage
    
    // Crossfade
    if(i > 0.9){
        stage += 1.0 - (1.0 - i) * 10.;
    }

    #ifdef OVERWRITE_STAGE
        stage = OVERWRITE_STAGE; // Overwrite stage
    #endif
    stage = mod(stage, float(numStages));
    
    /* AUDIO stage num */
    if(audio > 0 && texCoordVarying.s < 1 &&  texCoordVarying.t < 1){
        outputColor = vec4(vec3(stage < 1 ? 0 : 1), 1);
        // outputColor = vec4(1);
        return;
    }

    /* test white */
    if (debugMode == 0) {
        outputColor = vec4(1);
        return;
    }

    /* test strobe */
    // float q = .01;
    // if(mod(elapsedTimeMod,q) > (q/2)) {
    //     outputColor = vec4(1);
    // } else {
    //     outputColor = vec4(0,0,0,1);
    // }
    // return;
    
    /* test mask */
    if (debugMode == 1) {
        // if(mod(elapsedTimeMod / 2., 1) > 0.5){  
        outputColor = vec4(vec3(masked), 1);
        return;
        // } else {
        //     if (confidence < threshold) {
        //         outputColor = vec4(vec3(0.), 1.);
        //         return;
        //     } else {
        //         outputColor = vec4(1);
        //         return;
        //     }
        // }
    }

    /* test confidence */
    if (debugMode == 2) {
        outputColor = vec4(vec3(confidence > threshold ? 1.0 : 0.0),1.0);
        return;
    }

    /* Draw white border around edges */
    if (debugMode == 11) {
        vec2 pwh = getProjectorSize();
        vec2 p = getProjectorFragCoord();
        if (p.x < 1 || p.y < 1 || p.x > (pwh.x-1) || p.y > (pwh.y-1)) {
            outputColor = vec4(1.);
        } else {
            outputColor = vec4(vec3(0.), 1.);
        }
        return;
    }

    /* Color per projector */
    if (debugMode == 12) {
        int pn = getProjectorNum();
        if (pn == 0) {
            outputColor = vec4(1,0,0,1);
        } else if (pn == 1) {
            outputColor = vec4(0,1,0,1);
        } else if (pn == 2) {
            outputColor = vec4(0,0,1,1);
        }
        return;
    }

    /* Discard low confidence and masked */
    if(audio == 0 && (confidence < threshold || masked == 0)) {
        outputColor = vec4(vec3(0.), 1.);
        return;
    }

    /* Position debug mode */
    /* Position tester: */
    #ifdef TEST_POSITION_ALT
    int axis = TEST_POSITION_ALT;
    if(position[axis] > center_alt[axis] + sin(elapsedTimeMod) * 0.005){
        outputColor = vec4(1.,0.,0.,1.);
        return;
    } else {
        outputColor = vec4(0.,1.,0.,1.);
        return;
    }
    #endif

    int _debugMode = debugMode;
    #ifdef TEST_POSITION
    _debugMode = TEST_POSITION + 3;
    #endif
    if(_debugMode >= 3 && _debugMode <=5){
        int axis = _debugMode - 3;
        float dist = abs(position[axis] - center[axis]);
        if(dist < 0.003) {
            outputColor = vec4(0.,0.,1.,1.);
            return;
        }

        if(position[axis] > center[axis]){
            outputColor = vec4(1.,0.,0.,1.);
            return;
        } else {
            outputColor = vec4(0.,1.,0.,1.);
            return;
        }
    }

    if (debugMode == 6) {
        float c = hardStripes(0.0, position.y , 0.07, 0.1);
        outputColor = vec4(vec3(c), 1.0);
        return;
    }
    if (debugMode == 7) {
        float c = hardStripes(0.0, position.z , 0.07, 0.1);
        outputColor = vec4(vec3(c), 1.0);
        return;
    }
    if (debugMode == 8) {
        float c = smoothstep(-0.001, 0.001, centered.x);
        outputColor = vec4(vec3(c), 1.0);
        return;
    }
    if (debugMode == 9) {
        float c = smoothstep(-0.001, 0.001, centered.y);
        outputColor = vec4(vec3(c), 1.0);
        return;
    }
    if (debugMode == 10) {
        float c = smoothstep(-0.001, 0.001, centered.z);
        outputColor = vec4(vec3(c), 1.0);
        return;
    }

    /* Test masked white */
    // outputColor = vec4(1);
    // return;
    
    

    #ifdef MOUSE_BEAM
    vec3 beam = centered.xzy;
    float r = length(beam);
    float phi = atan(beam.x, beam.y); // -PI to PI
    float theta = acos(beam.z / r);

    // float cccc = 2.4-abs(phi - mouse.y*2*PI + PI);
    float cccc = 0.4-abs(phi - mouse.y*2*PI + PI);
    // return c * stageAlpha(s, stage);
    outputColor = vec4(vec3(smoothstep(0.0,0.01,cccc)), 1.0);
    // outputColor = vec4(1);
    return;
    #endif
    
    /*  Syphon render
        max side of syphon texture = max side of room */
    float syphonScale = max(syphonSize.x, syphonSize.y);
    
    /* use centered.xy it to sample across syphon texture
       this should lay an image on the floor
       change .xy to .yz or .xz to put it on two walls */    
    vec2 syphonCoord = syphonSize/2 + centered.xy * syphonScale;
   
    /* Output syphon */
    outputColor = texture(syphon, syphonCoord);
    // return; 
    
    
    float w = 0.;
    vec3 c = vec3(0.,0.,0.);
    
    
  
    int s = 0; //0
    
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        w += (lighthouse(elapsedTimeMod, position, centered)  > 0.5 ? 1. : 0.)
        * stageAlpha(s, stage);
    }
    
    s++; //1
    
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        float slantiness = 0.1;
        float baseSpeed = 0.4;
        // magic number 1.5488 is ratio between length to width of room
        // 2.5 creates alternating rising lines on walls, integer creates simultaneous flashes
        float lineSpacing = 1.5488 * 2.5;
        w += stripes(elapsedTimeMod + sin(elapsedTimeMod)
                     * baseSpeed, -position.z + position.y * slantiness, lineSpacing)
        * stageAlpha(s, stage);
    }
    
    s++; //2
    
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        w += glitter(elapsedTimeMod, centered)
        * stageAlpha(s, stage);
    }
    
    s++; //3 
    
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        w += smoothstep(0.0, 0.6, circles(sin(0.9 * elapsedTimeMod) * 1.8, centered, 160.))
        * stageAlpha(s, stage);
    }
    
    s++; //4


    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        centered.xz = rotate(centered.xz,  PI); // rotate seam to an appropriate wall

        float x = centered.x;
        float y = centered.z;
        float z = centered.y;
       
        float r = sqrt(x*x+y*y+z*z);
        float theta = atan(y,x);
        float phi = atan(sqrt(x*x+y*y),z);

        float rotationSpeed = 0.15;
        theta += elapsedTimeMod * rotationSpeed;

        float scale = 2;
        vec2 st = vec2(phi, theta) * scale;
        vec3 c = voronoi( st, 1, elapsedTimeMod );
        
        // borders
        float color = smoothstep( 0.11, 0.18, c.x );

        // feature points
        // float dd = length( c.yz );
        // color *= dd;
        
        w += color
            * stageAlpha(s, stage);;
    }

    
    s++; //5
   
    
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        w += (
            hardStripes(elapsedTimeMod * 0.15, position.z + position.x * 0.8, 0.2, 0.25)
            // + hardStripes(elapsedTimeMod * 0.15, position.z - position.x * 0.8, 0.2, 0.015)
        )
        // w += hardStripes(elapsedTimeMod * 0.10, position.z + position.y * 0.8, 0.1)
        // w += hardStripes(0 * 0.10, position.z + position.x * 0.8, 0.1)
        * stageAlpha(s, stage);
        // c.r = hardStripes(elapsedTimeMod * 0.3, position.z + position.y * 0.5, 0.3);
        // c.b = hardStripes(elapsedTimeMod * 0.2, position.z + position.y * 0.5, 0.3);
    }
    
    s++; //6
    
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        float cycleSpeed = 0.8;
        float maxSpeed = 1.8;
        float scale = 6;
        w += circles(sin(cycleSpeed * elapsedTimeMod) * maxSpeed, centered_stage , scale)
            * stageAlpha(s, stage);
    }
    
    s++; //7

    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        centered.xy = rotate(centered.xy, elapsedTimeMod * 0.1);
        centered.xz = rotate(centered.xz, elapsedTimeMod * 0.1);
        w += (
            hardStripes(elapsedTimeMod * 0.01, 
                sin(centered.y * 10 + elapsedTimeMod) * 0.05  
                + sin(centered.x * 23 + elapsedTimeMod) * 0.02
                + sin(centered.z * 23 + elapsedTimeMod) * 0.001 
                + centered.z 
                + centered.x * 0.1
                , 0.2, 
                (sin(centered.z * 10.0 + elapsedTimeMod + centered.y) + 1.0)*0.15)
            // + hardStripes(elapsedTimeMod * 0.01, sin(centered.x * 10 + elapsedTimeMod) * 0.05 + centered.x + centered.y * 0.1, 0.2, 0.3)
            // + hardStripes(elapsedTimeMod * 0.01, centered.y, 0.1, 0.07)
            // + hardStripes(elapsedTimeMod * 0.01, centered.x, 0.1, 0.07)
            // + hardStripes(elapsedTimeMod * 0.15, centered.z - centered.y, 0.2, 0.01)
        )
        * stageAlpha(s, stage);  
    }
    
    
    s++; //8
    
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        float ss = sin(elapsedTimeMod/1.0)/2.0;
        float cc = cos(elapsedTimeMod/1.3)/2.0;
        w += hardStripes(elapsedTimeMod * 0.07, position.z + position.x * (ss/ 5.0) + position.y * (cc/ 5.0), 0.15, 0.5)
        * stageAlpha(s, stage);;
    }
    
    s++; //9

    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        vec3 p = position - (vec3(cos(elapsedTimeMod*0.5) * 0.10,
                                sin(elapsedTimeMod*0.4) * 0.1,
                                sin(elapsedTimeMod*0.2) * 0.10 ) + center);

        w += (circles(elapsedTimeMod * - 1.5, p, 260.) * stageAlpha(s, stage) > 0.5 ? 1. : 0.);
    }
    
    s++; //10

    // Noise floor
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {   
        float c = perlin(position.zy, 100, elapsedTimeMod/3000000.);
        c = c > 0.5 ? 1. : 0.;
        w += c * stageAlpha(s, stage);
    }
    s++; //11

    // Noise radial lines
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {   
        float c = 0;
        for(int i=0; i<5;i++){
            float n = perlin(vec2(elapsedTimeMod/1000., elapsedTimeMod/1200.1), 109, i);
            c += radialLine(position.zy, centered.zy, n * PI * 5, 3.0);
        }
        w += c * stageAlpha(s, stage);
    }
    s++; //12

    // Noise radial lines
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {   
        float c = 0;
        for(int i=0; i<2;i++){
            float n = i * 3.1 + elapsedTimeMod / 15.;
            c += radialLine(position.xy, centered.xy, n * PI * 10, 2.5);
        }
        w += c * stageAlpha(s, stage);
    }
    s++; //13

     // Noise radial lines
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {   
        vec3 index = round(position/0.04);

        float c = 0;
        
        // c = perlin(vec2(index.x + cos(elapsedTimeMod)/109., index.y + sin(elapsedTimeMod)/120.1), 209, index.z);
        c = perlin(vec2(index.x + cos(elapsedTimeMod/10)/109., index.y + sin(elapsedTimeMod)/120.1), 209, index.z);

        c = c > 0.5 ? 1 : 0;
        // c = 1
        // c = index.y;

        // for(int i=0; i<2;i++){
            // float n = i * 3.1 + elapsedTimeMod / 15.;
            // c += radialLine(position.xy, centered.xy, n * PI * 10, 2.5);
        // }
        w += c * stageAlpha(s, stage);
    }
    s++; //14

    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        centered.xz = rotate(centered.xz, elapsedTimeMod * 0.1);
        w += (
            hardStripes(elapsedTimeMod * 0.15, centered.z + centered.y, 0.2, audio == 0 ? 0.01 : 0.09)
            + hardStripes(elapsedTimeMod * 0.15, centered.z - centered.y, 0.2, audio == 0 ? 0.01 : 0.09)
        )
        * stageAlpha(s, stage);;
        
    }
    
    s++; //15
    
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
       float c = 0;
        for(int i=0; i<8;i++){
            float n = perlin(vec2(elapsedTimeMod/2500., elapsedTimeMod/2200.1), 109, i);
            c += radialLine(position.xz, centered.xz, n * PI * 10, 2.8);
        }
        w += c * stageAlpha(s, stage);
    }
    
    s++; //16
       
    if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
        w += unstableFloor(elapsedTimeMod, centered_stage.zy, 150.)
        // w += unstableFloor(1, centered.yx, 150.)
        * stageAlpha(s, stage);;
    }
    
    // s++; //17
       
    // if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {
    //     // float r = perlin(position.xz + vec2(elapsedTimeMod * 0.0), 50, 0);
    //     float ry = abs(0.5-mod(position.y + elapsedTimeMod * 0.3 + perlin(position.xz, 10, 0) , 1.0));

        
    //     float c =step(ry,0.03);// step( ry, 0.1);

    //     w += c
    //     // w += unstableFloor(1, centered.yx, 150.)
    //     * stageAlpha(s, stage);;
    // }


    // // Mouse Beams
    // if(stageAlpha(s, stage) > 0. && stageAlpha(s, stage) <= 1.) {   
    //     vec3 beam = calculateBeamVector(texCoord);
    //     float r = length(beam);
        
    //     float phi = atan(beam.x, beam.y); // -PI to PI
    //     float theta = acos(beam.z / r);

    //     float c = 1-abs(theta - mouse.y*2*PI + PI);
    //     w += c * stageAlpha(s, stage);
    // }
    // s++; //15

    /* Stripes test */
    // // w = hardStripes(elapsedTimeMod * 0.1, position.x, 0.1) ;
    // w = 1;
    // c *= 0.5;
    // // outputColor += vec4(vec3(w) * 0.5 * vec3(0.5, 0.5, 1.0) , 1.);
    
    // Set color from the stagess
    outputColor = vec4(vec3(w) * 1.0 , 1.);
    
    // avoid a bug that causes some pixels to end up on the far end
    // if(position.x == 0){
    //     outputColor = vec4(0.);
    // }

    /* Test beam direction */
    // vec3 beam = calculateBeamVector(texCoord) / length(calculateBeamVector(texCoord));
    // outputColor = vec4(vec3(-beam.z) , 1);

    // force alpha = 1
    // outputColor.a = 1.;
}

