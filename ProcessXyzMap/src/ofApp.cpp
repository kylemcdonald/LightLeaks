#include "ofApp.h"

void ofApp::setup() {
	ofSetLogLevel(OF_LOG_VERBOSE);
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	shader.setup("shader");
	xyzMap.loadImage("xyzMap.exr");
}

void ofApp::update() {
	
}

void ofApp::draw() {
	ofBackground(0);
	shader.begin();
	shader.setUniform1f("elapsedTime", ofGetElapsedTimef());
	shader.setUniformTexture("xyzMap", xyzMap, 0);
	xyzMap.draw(0, 0);
	shader.end();
}

void ofApp::keyPressed(int key) {
	if(key == ' ') {
		//shader.load("shader");
	}
}