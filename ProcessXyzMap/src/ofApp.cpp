#include "ofApp.h"

void ofApp::setup() {
	ofSetLogLevel(OF_LOG_VERBOSE);
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	shader.setup("shader");
    
    img.loadImage("test.png");
    
	xyzMap.loadImage("xyzMap.exr");
}

void ofApp::update() {

	
}

void ofApp::draw() {
	ofBackground(0);
	shader.begin();
	shader.setUniform1f("elapsedTime", ofGetElapsedTimef());
	shader.setUniformTexture("xyzMap", xyzMap, 0);
    shader.setUniformTexture("texture", img.getTextureReference(), 1);
	xyzMap.draw(0, 0);
	shader.end();
}

void ofApp::keyPressed(int key) {
	if(key == ' ') {
        room = !room;
        
        if(room){
            xyzMap.loadImage("xyzMapRoom.exr");

        } else {
            xyzMap.loadImage("xyzMap.exr");
        }
		//shader.load("shader");
	}
    if(key == 'f'){
        ofToggleFullscreen();
    }
}