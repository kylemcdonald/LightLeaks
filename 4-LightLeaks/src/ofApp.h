#pragma once

#include "ofMain.h"
#include "ofxAssimpModelLoader.h"
#include "ofxCv.h"
#include "ofAutoShader.h"
#include "ofxBlackMagic.h"
#include "CoordWarp.h"
#include "ofxXmlSettings.h"
#include "ofxBiquadFilter.h"
#include "ofxOsc.h"

enum Stage {
    Lighthouse=0,
    Spotlight,
    Intermezzo
};

class ofApp : public ofBaseApp {
public:
    void setup(), setupSpeakers(), setupTracker();
    void update(), updateTracker(), updateOsc();
	void draw();
    void exit();
	void keyPressed(int key);
    void mouseMoved(int x, int y);
    void mousePressed( int x, int y, int button );
    
    void updateCameraCalibration();
    
    bool debugMode;
    
    float dt, previousTime;
	
	ofFloatImage xyzMap;
    ofFloatImage normalMap;
    ofFloatImage confidenceMap;
	ofAutoShader shader;
    
    //Settings
    ofxXmlSettings settings;
    
    void startStage(Stage stage);
    Stage stage;
    Stage stageGoal;
    
    float stageAge;
    float stageAmp;

    //Lighthouse
    float lighthouseAngle;

    //Spotlight
    float spotlightThresholder;
    ofxBiquadFilter2f spotlightPosition;
    
    //Speaker sampling
    ofFloatImage speakerXYZMap;
    ofFbo speakerFbo;
    ofFloatPixels speakerPixels;
    float speakerAmp[4];
    
    //Tracking
    ofxBlackMagic grabber;
    int photoCounter;
    cv::Mat grabberSmall, grabberThresholded;
    
    ofxCv::RunningBackground cameraBackground;
    ofxCv::ContourFinder contourFinder;
    coordWarping cameraCalibration;
    ofVec2f cameraCalibrationCorners[4];
    bool firstFrame;
    int setCorner;
    
    //OSC
    ofxOscSender oscSender;
};