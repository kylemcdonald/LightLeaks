#pragma once

#include "ofMain.h"
#include "ofxAssimpModelLoader.h"
#include "ofxCv.h"
#include "ofAutoShader.h"

class ofApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();
	
	ofxAssimpModelLoader model;
	ofMesh mesh;
	ofEasyCam cam;
	ofShader shader;
	float range;
	ofVec3f zero, center;
	ofFbo fbo;
	ofImage referenceImage;
};