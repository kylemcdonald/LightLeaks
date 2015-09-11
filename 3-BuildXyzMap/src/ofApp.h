#pragma once

#include "ofMain.h"
#include "ofxCv.h"
#include "ofxAssimpModelLoader.h"


class ofApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();
	
	cv::Mat proXyzCombined, proNormalCombined, proConfidenceCombined;
    
    ofEasyCam cam;
    ofxAssimpModelLoader model;
    ofVboMesh objectMesh;
    ofShader xyzShader;
    ofShader normalShader;
    
    float range;
    ofVec3f zero;
    
    ofVbo pointCloud;
    
    ofMesh mesh;
    ofMesh referencePointsMesh;
    
    ofColor colors[10];
    
    ofMatrix4x4 modelMatrix;
    ofxCv::Intrinsics intrinsics;
    
    ofFbo xyzFbo, normalFbo, debugFbo;
    


};