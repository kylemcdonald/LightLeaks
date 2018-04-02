varying vec3 pos;
uniform vec3 zero;
uniform float range;

void main() {
	gl_FragColor = vec4((pos.xyz - zero) / range, 1.);
}
