varying vec3 normal;

void main() {
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	normal = gl_Normal.xyz;
}
