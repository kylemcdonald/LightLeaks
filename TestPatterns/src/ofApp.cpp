#include "ofApp.h"

void ofApp::setup() {
	shader.setup("shader");
}

void ofApp::update() {
	
}

void ofApp::draw() {
	shader.begin();
	shader.setUniform1f("elapsedTime", ofGetElapsedTimef());
	ofRect(0, 0, ofGetWidth(), ofGetHeight());
	shader.end();
}

void ofApp::keyPressed(int key) {
	if(key == 'f') {
		ofToggleFullscreen();
	}
}