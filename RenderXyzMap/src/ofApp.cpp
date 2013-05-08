#include "ofApp.h"

void getBoundingBox(const ofMesh& mesh, ofVec3f& min, ofVec3f& max) {
	int n = mesh.getNumVertices();
	if(n > 0) {
		min = mesh.getVertex(0);
		max = mesh.getVertex(0);
		for(int i = 1; i < n; i++) {
			const ofVec3f& cur = mesh.getVertices()[i];
			min.x = MIN(min.x, cur.x);
			min.y = MIN(min.y, cur.y);
			min.z = MIN(min.z, cur.z);
			max.x = MAX(max.x, cur.x);
			max.y = MAX(max.y, cur.y);
			max.z = MAX(max.z, cur.z);
		}
	}
}

void ofApp::setup() {
	ofSetLogLevel(OF_LOG_VERBOSE);
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	shader.load("xyz.vs", "xyz.fs");
	model.loadModel("room.dae");
	mesh = model.getMesh(0);
	ofVec3f min, max;
	getBoundingBox(mesh, min, max);
	zero = min;
	ofVec3f diagonal = max - min;
	range = MAX(MAX(diagonal.x, diagonal.y), diagonal.z);
	center = (min + max) / 2;
	cout << "center: " << center << endl;
	cout << "range: " << range << endl;
	cout << "center normalized: " << (center / range) << endl;
	
	referenceImage.loadImage("referenceImage.png");
	
	ofFbo::Settings settings;
	settings.width = referenceImage.getWidth();
	settings.height = referenceImage.getHeight();
	settings.useDepth = true;
	settings.internalformat = GL_RGBA32F_ARB;
	fbo.allocate(settings);
}

void ofApp::update() {
	
}

void ofApp::draw() {
	fbo.begin();
	ofClear(0);
	glEnable(GL_DEPTH_TEST);
	cam.begin();
	shader.begin();
	shader.setUniform1f("range", range);
	shader.setUniform3fv("zero", zero.getPtr());
	mesh.drawFaces();
	shader.end();
	cam.end();
	fbo.end();
	
	glDisable(GL_DEPTH_TEST);
	ofBackground(0);
	ofSetColor(255);
	fbo.draw(0, 0);
	if(ofGetKeyPressed(' ')) {
		referenceImage.draw(0, 0);
	}
}