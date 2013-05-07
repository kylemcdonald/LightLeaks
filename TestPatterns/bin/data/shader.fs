uniform float elapsedTime;

void main() {
	float speed = 10.;
	float scale = 10.;
	gl_FragColor = vec4(vec3(abs(sin(elapsedTime * speed + gl_FragCoord.x / scale))), 1.);
}
