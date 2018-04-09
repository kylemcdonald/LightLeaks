varying vec3 normal;

void main() {
	gl_FragColor = vec4((normal.xyz + 1.) / 2., 1.);
}
