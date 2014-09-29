#pragma once 

#include "ofMain.h"
#include "ofxOpenCv.h"

class coordWarping{
    
    
public:
    
    //---------------------------
    coordWarping();
    ~coordWarping();
    
    void calculateMatrix(ofVec2f src[4], ofVec2f dst[4]);
    
    ofVec2f transform(float xIn, float yIn);
    ofVec2f inversetransform(float xIn, float yIn);
    
    ofVec2f transform(ofVec2f p);
    ofVec2f inversetransform(ofVec2f p);
    
    CvMat *translate;
    CvMat *itranslate;
    
protected:
    
    CvPoint2D32f cvsrc[4];
    CvPoint2D32f cvdst[4];
    
};