#pragma once

#include "ofMain.h"
#include "ofxCv.h"
#include "ofxAssimpModelLoader.h"


class ofApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();
    void dragged(ofDragInfo & drag);
    void keyPressed( int key );

    void autoCalibrateXyz(string path, cv::Mat proConfidenceMat, cv::Mat proMapMat);
    void processScan(ofFile scanName);
    void saveResult();
    
	
	cv::Mat proXyzCombined, proXyzTotalCombined, proNormalCombined, proConfidenceCombined;
    
    ofEasyCam cam;
    ofxAssimpModelLoader model;
    ofVboMesh objectMesh;
    ofShader xyzShader;
    //ofShader normalShader;
    
    float range;
    ofVec3f zero;
    
    ofVbo pointCloud;
    
    ofMesh mesh, meshOutput;
    ofMesh referencePointsMesh;
    
    ofColor colors[10];
    int colorCounter;
    
    ofMatrix4x4 modelMatrix;
    ofxCv::Intrinsics intrinsics;
    
    ofFbo xyzFbo, normalFbo, debugFbo;
    

    float confidenceThreshold;
    float viewBetternes;
    int scaleFactor;
    
    
    bool totalFound;
    string statusText;

    
    ofImage debugViewOutput;

};