#include "ofApp.h"

void ofApp::setup() {
	ofSetLogLevel(OF_LOG_VERBOSE);
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	shader.setup("shader");
    
    img.loadImage("image.png");
    
	xyzMap.loadImage("xyzMap.exr");
    normalMap.loadImage("normalMap.exr");
    confidenceMap.loadImage("confidenceMap.exr");
    
    wallFbo.allocate(2048, 200);
}

void ofApp::update() {

	
}

void ofApp::draw() {
	ofBackground(0);
    
    
    wallFbo.begin();
  /*  ofClear(0, 0, 0);
    ofSetColor(255, 255, 255);
    for(int y=0;y<wallFbo.getHeight(); y+= 20){
            for(int x=0;x<wallFbo.getWidth(); x+= 20){
                ofRect(x, y, 10, 10);
                ofRect(x+10, y+10, 10, 10);
            }
    }*/
//    image.draw()
    wallFbo.end();
    
    
    
	shader.begin();
	shader.setUniform1f("elapsedTime", ofGetElapsedTimef());
	shader.setUniform2f("textureSize", img.getWidth(), img.getHeight());
	shader.setUniformTexture("xyzMap", xyzMap, 0);
	shader.setUniformTexture("normalMap", normalMap, 2);
    shader.setUniformTexture("confidenceMap", confidenceMap, 3);
    shader.setUniformTexture("texture", img.getTextureReference(), 1);
	xyzMap.draw(0, 0);
	shader.end();
    
    //wallFbo.draw(0, 0, ofGetWidth(), ofGetHeight());
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