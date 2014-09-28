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

 const vec4 on = vec4(1.);
 const vec4 off = vec4(vec3(0.), 1.);

 const vec3 center = vec3(0, 0, 0); 

 const float waves = 19.;



 uniform float blackbody_color[273] = float[273](
 	1.0000, 0.0425, 0.0000, /* 1000K */
 	1.0000, 0.0668, 0.0000, /* 1100K */
 	1.0000, 0.0911, 0.0000, /* 1200K */
 	1.0000, 0.1149, 0.0000, /* ... */
 	1.0000, 0.1380, 0.0000,
 	1.0000, 0.1604, 0.0000,
 	1.0000, 0.1819, 0.0000,
 	1.0000, 0.2024, 0.0000,
 	1.0000, 0.2220, 0.0000,
 	1.0000, 0.2406, 0.0000,
 	1.0000, 0.2630, 0.0062,
 	1.0000, 0.2868, 0.0155,
 	1.0000, 0.3102, 0.0261,
 	1.0000, 0.3334, 0.0379,
 	1.0000, 0.3562, 0.0508,
 	1.0000, 0.3787, 0.0650,
 	1.0000, 0.4008, 0.0802,
 	1.0000, 0.4227, 0.0964,
 	1.0000, 0.4442, 0.1136,
 	1.0000, 0.4652, 0.1316,
 	1.0000, 0.4859, 0.1505,
 	1.0000, 0.5062, 0.1702,
 	1.0000, 0.5262, 0.1907,
 	1.0000, 0.5458, 0.2118,
 	1.0000, 0.5650, 0.2335,
 	1.0000, 0.5839, 0.2558,
 	1.0000, 0.6023, 0.2786,
 	1.0000, 0.6204, 0.3018,
 	1.0000, 0.6382, 0.3255,
 	1.0000, 0.6557, 0.3495,
 	1.0000, 0.6727, 0.3739,
 	1.0000, 0.6894, 0.3986,
 	1.0000, 0.7058, 0.4234,
 	1.0000, 0.7218, 0.4485,
 	1.0000, 0.7375, 0.4738,
 	1.0000, 0.7529, 0.4992,
 	1.0000, 0.7679, 0.5247,
 	1.0000, 0.7826, 0.5503,
 	1.0000, 0.7970, 0.5760,
 	1.0000, 0.8111, 0.6016,
 	1.0000, 0.8250, 0.6272,
 	1.0000, 0.8384, 0.6529,
 	1.0000, 0.8517, 0.6785,
 	1.0000, 0.8647, 0.7040,
 	1.0000, 0.8773, 0.7294,
 	1.0000, 0.8897, 0.7548,
 	1.0000, 0.9019, 0.7801,
 	1.0000, 0.9137, 0.8051,
 	1.0000, 0.9254, 0.8301,
 	1.0000, 0.9367, 0.8550,
 	1.0000, 0.9478, 0.8795,
 	1.0000, 0.9587, 0.9040,
 	1.0000, 0.9694, 0.9283,
 	1.0000, 0.9798, 0.9524,
 	1.0000, 0.9900, 0.9763,
 	1.0000, 1.0000, 1.0000, /* 6500K */
 	0.9771, 0.9867, 1.0000,
 	0.9554, 0.9740, 1.0000,
 	0.9349, 0.9618, 1.0000,
 	0.9154, 0.9500, 1.0000,
 	0.8968, 0.9389, 1.0000,
 	0.8792, 0.9282, 1.0000,
 	0.8624, 0.9179, 1.0000,
 	0.8465, 0.9080, 1.0000,
 	0.8313, 0.8986, 1.0000,
 	0.8167, 0.8895, 1.0000,
 	0.8029, 0.8808, 1.0000,
 	0.7896, 0.8724, 1.0000,
 	0.7769, 0.8643, 1.0000,
 	0.7648, 0.8565, 1.0000,
 	0.7532, 0.8490, 1.0000,
 	0.7420, 0.8418, 1.0000,
 	0.7314, 0.8348, 1.0000,
 	0.7212, 0.8281, 1.0000,
 	0.7113, 0.8216, 1.0000,
 	0.7018, 0.8153, 1.0000,
 	0.6927, 0.8092, 1.0000,
 	0.6839, 0.8032, 1.0000,
 	0.6755, 0.7975, 1.0000,
 	0.6674, 0.7921, 1.0000,
 	0.6595, 0.7867, 1.0000,
 	0.6520, 0.7816, 1.0000,
 	0.6447, 0.7765, 1.0000,
 	0.6376, 0.7717, 1.0000,
 	0.6308, 0.7670, 1.0000,
 	0.6242, 0.7623, 1.0000,
 	0.6179, 0.7579, 1.0000,
 	0.6117, 0.7536, 1.0000,
 	0.6058, 0.7493, 1.0000,
 	0.6000, 0.7453, 1.0000,
 	0.5944, 0.7414, 1.0000 /* 10000K */
 	);


 vec3 interpolate_color(float a, int c1, int c2)
 {
 	vec3 c;
 	c.x = (1.0-a) * blackbody_color[c1] + a * blackbody_color[c2];
 	c.y = (1.0-a) * blackbody_color[c1+1] + a * blackbody_color[c2+1];
 	c.z = (1.0-a) * blackbody_color[c1+2] + a * blackbody_color[c2+2];
 	return c;
/*
    vec3 c;
    c.x = blackbody_color[c1];
 	c.y = blackbody_color[c1+1];
    c.z = blackbody_color[c1+2];

    return c;
    */     
}

vec3 colorTemp(float temp, float intensity){
    float temp_index = (temp-1000.)/100.0;//((temp - 1000.) / 100.);
    //temp_index = 0.;
    float alpha = mod(temp , 100.) / 100.0;
  //	float alpha = intensity;

  vec3 color = intensity * interpolate_color(alpha, int(temp_index)*3, int(temp_index)*3+3);
/*
*/

 //   cout<<temp_index<<"   "<<blackbody_color[temp_index]*intensity<<endl;
  //  color.set(blackbody_color[temp_index]*intensity, blackbody_color[temp_index+1]*intensity, blackbody_color[temp_index+2]*intensity);
  return color;
}




// triangle wave from 0 to 1
float wrap(float n) {
	return abs(mod(n, 2.)-1.)*-1. + 1.;
}

// creates a cosine wave in the plane at a given angle
float wave(float angle, vec2 point) {
	float cth = cos(angle);
	float sth = sin(angle);
	return (cos (cth*point.x + sth*point.y) + 1.) / 2.;
}

// sum cosine waves at various interfering angles
// wrap values when they exceed 1
float quasi(float interferenceAngle, vec2 point) {
	float sum = 0.;
	for (float i = 0.; i < waves; i++) {
		sum += wave(3.1416*i*interferenceAngle, point);
	}
	return wrap(sum);
}

float animate00(float stage){
	float a = mod(stage, 1);
	
	//Range 0...1

	a -= 0.5;
	a *= 2.;

	//Range -1...1

	a = pow(a,2.);

	//Range -1...1

	a += 1.;
	a *= 0.5;

	//Range 0 ... 0.5 ... 0 
	a = 1-a;
	return a;
}


float animate01(float stage){
	float a = mod(stage, 1);
	
	//Range 0...1

	a -= 0.5;
	a *= 2.;

	//Range -1...1

	a = pow(a,3.);

	//Range -1...1

	a += 1.;
	a *= 0.5;

	//Range 0...1

	return a;
}

vec2 rotate(vec2 position, float amount) {
	mat2 rotation = mat2(
        vec2( cos(amount),  sin(amount)),
        vec2(-sin(amount),  cos(amount))
    );
    return rotation * position;
}

void main() {
	vec2 overallOffset = vec2(0);//+vec2(floor( sin(elapsedTime*1)*10) ,0);
	vec4 curSample = texture2DRect(xyzMap, gl_TexCoord[0].st+overallOffset);
	vec4 curSampleNormal = texture2DRect(normalMap, gl_TexCoord[0].st+overallOffset);
	vec4 curSampleConfidence = texture2DRect(confidenceMap, gl_TexCoord[0].st+overallOffset);
	
	vec3 position = curSample.xyz;
	vec3 normalDir = curSampleNormal.xyz;
	float confidence = curSampleConfidence.x;
	//position.xy += +overallOffset;
	float present = curSample.a;

	/*if(present == 0.) {
		gl_FragColor = vec4(0.);
		return;
	}*/

	float stages = 11.;
	float stage = 7;//+mod(elapsedTime * .3, stages);

	gl_FragColor = vec4(0);

	if(confidence < .1) {
	gl_FragColor = vec4(0);
} else {
	gl_FragColor = vec4(vec3(mod(elapsedTime * 2 + length(position) * 20, 1) > .8 ? 1 : 0), 1);
	}
	return;

	/*{
		//if(present == 0 || position.z < 0.01) return;
		float speed = 0.35;
		int i = int(gl_TexCoord[0].x/1024);
		gl_FragColor = (mod(elapsedTime+i*speed/3.,speed) > speed*0.66666) ? on : off;
		return;
	}*/
	/*{
		vec2 offset = vec2(0.2, -0.1);
		vec2 tc = position.xy + offset;
  		vec2 p = -1.0 + 2.0 * tc;
  		float len = length(p);
  		
  		vec2 uv = tc + (p/len)*cos(len*12.0- elapsedTime *4.0)*0.02 - offset;
  		vec3 col = texture2DRect(texture,vec2(0,0)+uv*1024.).xyz;
  		if(present> 0)
  			gl_FragColor = vec4(1);//vec4(col,1.0);


		return;
	}
*/
	if(confidence < 0.1) return;

	if(position.z > 0 || (stage <= 9 && stage > 8)  || (stage <= 11 && stage > 10)){
		
		if(stage <= 1.) {

			// diagonal stripes
			float speed = 100;
			const float scale = .00001 ;
			float a = (1.-animate01(stage))*0.2;
			//float a = scale/2.;

			vec2 rotated = rotate(position.xy, elapsedTime);
			gl_FragColor = (mod((rotated.x*mod(stage,1)+ rotated.y + position.z) + (elapsedTime * speed), scale) > a) ?
			on : off;



		} else if(stage <= 2.) {
			// fast rising stripes
			//if(normal.z == 0.) {
			float a = 1.-mod(stage,1);
			float speed = .01;

			float height = center.z * 2.;

			gl_FragColor = (((1-position.y * (position.x) * 1./height) < ( a ))) ? on : off;

		} else if(stage <= 3.) {
			// crazy triangles, grid lines
			if(  normalDir.z < 0.5){
				float speed = .05;
				float scale = .05;
				float cutoff = 1.-animate00(stage)*0.2;
				vec3 cur = mod(position + speed * elapsedTime, scale) / scale;
				cur *= 1. - abs(normal);
				/*if(stage < 2.) {
					gl_FragColor = ((cur.x + cur.y + cur.z) < cutoff) ? off : on;
				} else {*/
				gl_FragColor = (max(max(cur.x, cur.y), cur.z) < cutoff) ? off : on;
			}
			//}
		
		} else if(stage <= 4.) {
			// one line thorugh the room
			// TODO: different angles 
			vec2 divider = vec2(cos(elapsedTime*2.), sin(elapsedTime*2.));
			float side = (position.x * divider.y) - (position.y * divider.x);

			gl_FragColor = abs(side) < .01 + 0.1 * sin(elapsedTime * 2.) ? on : off;
		} else if(stage <= 5.){
				vec2 normPosition = (position.xz + position.yx) / 100.;
				float b = 0.3*(sin(elapsedTime*3.0+30.0*position.y)+1.);
				//gl_FragColor = vec4(vec3(b), 1.);

				//Color
				vec3 color = colorTemp(4000. + (1-b+0.3) * 5000.,1.0);

				gl_FragColor = vec4(color*b*animate00(stage), 1.);
		} /*else if(stage <= 6.){
			//Tilting room

				float t = sin(elapsedTime)*.1;
				vec2 fromCenter = center.yz - position.yz;
				vec2 rot = vec2(sin(t), cos(t));

				float b = sin(50.*dot(rot, fromCenter));
			
			//float c = 0.5*(sin(b+elapsedTime)+1.);
				gl_FragColor = vec4(vec3(b)*animate00(stage), 1.);		
		} else if(stage <= 6.){
				float t = elapsedTime;
				vec2 fromCenter = center.yx - position.yx;
				vec2 rot = vec2(sin(t), cos(t));

				float r = (1.+sin(4.*dot(rot, fromCenter)))*0.5;
				
				vec3 color = colorTemp(animate00(stage)*4000. +2000. + r * 5000.,1.);

				gl_FragColor = vec4(color*animate00(stage), 1.);		

		} */else if(stage <= 6.){
				float t = elapsedTime;
				vec2 fromCenter = center.yx - position.yx;
				vec2 rot = vec2(sin(t), cos(t));

				float r = sin(2.*dot(rot, fromCenter))*animate00(stage);
				float g = sin(4.*dot(rot, fromCenter))*animate00(stage);
				float b = sin(6.*dot(rot, fromCenter))*animate00(stage);


				gl_FragColor = vec4(r,g,b, 1.);		
		} else if(stage <= 7.){
				//Spheres
				vec3 c = center;
				c.x += 0.05;
				c.y -= 0.02;
				vec3 fromCenter = c.xyz - position.xyz;
				float b = sin(500.*mod(length(fromCenter)+(0.02*sin(elapsedTime*1.)), 10.));

				gl_FragColor = vec4(vec3(b), 1.);

		} /*else if(stage <= 13.) {
				vec2 normPosition = (position.xz + position.yx) / 100.;
				float b = quasi(elapsedTime*0.04, (normPosition)*200.);
				gl_FragColor = vec4(vec3(b), 1.);
		} *//*else if(stage <= 8){ 
			// Text on end wall
			if(position.y == center.y * 2.){
				vec2 samplePos = position.xz;
				samplePos.x -= sin(elapsedTime)* 0.02;
				samplePos.y -= 0.05;
				samplePos *= 2.;
				samplePos *= 1024.;
				vec4 curSample = texture2DRect(texture, samplePos);

				gl_FragColor = curSample;
			}

		}*/
		else if(stage <= 8){ 
			// Text on ceilling
			//if(position.y == center.y * 2.){
				vec2 samplePos = position.yx - center.yx;
				samplePos.y *= -1;
				samplePos *= textureSize.x * mod(elapsedTime, 20);
				samplePos += textureSize / 2;
				//sampl
				vec4 curSample = texture2DRect(texture, samplePos);

				gl_FragColor = curSample;
				//gl_FragColor = vec4(position.x*2, position.y, position.z*3, 1);
			//}

		}
		else if(stage <= 9) {
			if(gl_TexCoord[0].x < 1024.) {
				gl_FragColor = vec4(1., 0., 0., 1.);
			} else if(gl_TexCoord[0].x < 2048.) {
				gl_FragColor = vec4(0., 1., 0., 1.);
			} else {
				gl_FragColor= vec4(0., 0., 1., 1.);
			}
		}
		else if(stage <= 10) {
			if(normalDir.z < 0.5){
				float spacing = sin(elapsedTime) * .8 + 1;
				vec2 offset = rotate(vec2(1, 0), .6 * elapsedTime);
				float result = length(mod(position.xy + offset, spacing) * 2 - spacing / 2);
				gl_FragColor = (result < spacing / 2) ? on : off;
			}
		} else if(stage <= 11){
			float i = mod(stage,1);

			vec2 screenSpace = gl_TexCoord[0].xy - vec2(512,400);
			screenSpace /= 2000.;
			vec2 worldSpace = position.xy;
			worldSpace -= center.xy;
			vec2 mixedSpace = mix(screenSpace, worldSpace, i);
			if(length(mixedSpace) < .1) {
				gl_FragColor =  on;
				if(present == 0)
					gl_FragColor = vec4(vec3(1-i), 1);
			}
		}

		/*else if(stage <= 4.) {
			// moving outlines 
			const float speed = 10.;
			const float scale = 6.;
			float localTime = 5. * randomOffset + elapsedTime;
			gl_FragColor = 
			(mod((-position.x - position.y + position.z) + (localTime * speed), scale) > scale / 2.) ?
			on : off;
		} */
	}
}

