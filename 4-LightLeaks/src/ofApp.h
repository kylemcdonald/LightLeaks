#pragma once

#include "ofMain.h"
#include "ofxAssimpModelLoader.h"
#include "ofxCv.h"
#include "ofAutoShader.h"
#include "CoordWarp.h"
#include "ofxXmlSettings.h"
#include "ofxBiquadFilter.h"
#include "ofxOsc.h"

//#define USE_CAMERA

#ifdef USE_CAMERA
#include "ofxBlackMagic.h"
#endif

enum Stage {
    Lighthouse=0,
    Spotlight,
    Intermezzo,
    Linescan
};

class ofApp : public ofBaseApp {
public:
    void setup(), setupSpeakers();
    void update(), updateOsc();
	void draw();
    void exit();
	void keyPressed(int key);
    void mouseMoved(int x, int y);
    void mousePressed( int x, int y, int button );
    
    bool debugMode;
    int debugStage;
    
    
    float dt, previousTime;
	
	ofFloatImage xyzMap;
    ofFloatImage normalMap;
    ofFloatImage confidenceMap;
	ofShader shader;
    
    //Settings
    ofxXmlSettings settings;
    ofXml config;
    
    void startStage(Stage stage);
    Stage stage;
    Stage stageGoal;
    int substage;
    
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
    
    ofImage kl;
    
    //Scanlines
    int scanDir;
    
    bool setupCalled;
    vector< shared_ptr<ofAppBaseWindow> > windows;
    
#ifdef USE_CAMERA
    void setupTracker(), updateTracker();
    void updateCameraCalibration();
    void logAudience();
    
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
#endif
    
    //OSC
    ofxOscSender oscSender;
};