#pragma once

#include "ofMain.h"

#include "ofxCv.h"
#include "ofxGrabCam.h"
#include "ofxAssimpModelLoader.h"

class ofApp : public ofBaseApp{

public:
	void setup();
	void update();
    void updateCalibration();
	void draw();
    void drawMeshView();
    void drawImageView();
    void drawCalibratedMesh();
	void keyPressed(int key);
    void mousePressed(int x, int y, int button);
    void mouseDragged(int x, int y, int button);
    
    ofxAssimpModelLoader loader;
    ofMesh mesh;
    ofImage image;

	ofxGrabCam camera;
    
    bool meshView = true;
    
    float radius = 10;
    bool hovering = false;
    bool selected = false;
    ofVec2f hoveringPosition;
    ofVec2f selectedPosition;
    unsigned int hoveringIndex = 0;
    unsigned int selectedIndex = 0;
    
    std::map<unsigned int, ofVec2f> mappedIndices;
    
    ofMatrix4x4 cameraMatrix, modelMatrix;
    bool calibrationReady = false;
};
