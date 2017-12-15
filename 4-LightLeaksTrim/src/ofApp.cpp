#include "ofApp.h"

void ofApp::setup() {
    ofSetLogLevel(OF_LOG_VERBOSE);
    ofSetVerticalSync(true);
    
    shader.loadAuto("shader");
    
    xyzMap.loadAuto("../../../SharedData/xyzMap.exr");
    xyzMap.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
    
    confidenceMap.loadAuto("../../../SharedData/confidenceMap.exr");
    confidenceMap.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
    
    calibrationMap.loadAuto("../../../SharedData/calibrationIndexMap.png");
    calibrationMap.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);

    ofLog() << xyzMap.getWidth() << " x " << xyzMap.getHeight();
    ofLog() << confidenceMap.getWidth() << " x " << confidenceMap.getHeight();
}

void ofApp::update() {
}

void ofApp::draw() {
    ofBackground(0);
    ofEnableAlphaBlending();
    ofSetColor(255);
    
    shader.begin(); {
        shader.setUniformTexture("xyzMap", xyzMap, 0);
        shader.setUniformTexture("confidenceMap", confidenceMap, 1);
        shader.setUniformTexture("calibrationMap", calibrationMap, 2);
        shader.setUniform1f("elapsedTime", ofGetElapsedTimef());
        shader.setUniform2f("mouse", ofVec2f((float)mouseX/ofGetWidth(), (float)mouseY/ofGetHeight()));
        xyzMap.draw(0, 0, ofGetWidth(), ofGetHeight());
    } shader.end();
}

void ofApp::keyPressed(int key) {
    if(key == 'f'){
        ofToggleFullscreen();
    }
}
