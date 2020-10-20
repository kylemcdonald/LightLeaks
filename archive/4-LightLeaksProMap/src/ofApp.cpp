#include "ofApp.h"

void ofApp::setup() {
    ofSetLogLevel(OF_LOG_VERBOSE);
    ofSetVerticalSync(true);
    
    shader.loadAuto("../../../SharedData/shader/shader");
    
    proMap.loadAuto("../../../SharedData/scan-1811-center/proMap.png");
    proMap.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
    
    proConfidence.loadAuto("../../../SharedData/scan-1811-center/proConfidence.exr");
    proConfidence.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
}

void ofApp::update() {
}

void ofApp::draw() {
    ofBackground(0);
    ofEnableAlphaBlending();
    ofSetColor(255);
    ofHideCursor();
    
    shader.begin(); {
        shader.setUniformTexture("proConfidence", proConfidence, 0);
        shader.setUniformTexture("proMap", proMap, 1);
        shader.setUniform1f("elapsedTime", ofGetElapsedTimef());
        proMap.draw(0, 0);
    } shader.end();
}

void ofApp::keyPressed(int key) {
    if(key == 'f'){
        ofToggleFullscreen();
    }
}
