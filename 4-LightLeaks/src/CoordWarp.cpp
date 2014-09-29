#include "CoordWarp.h"
//---------------------------
coordWarping::coordWarping(){
    translate = cvCreateMat(3,3,CV_32FC1);
    itranslate = cvCreateMat(3,3,CV_32FC1);
    
}

coordWarping::~coordWarping(){
    cvReleaseMat(&translate);
    cvReleaseMat(&itranslate);
}

//---------------------------
void coordWarping::calculateMatrix(ofVec2f src[4], ofVec2f dst[4]){
    cvSetZero(translate);
    cvSetZero(itranslate);
    
    for (int i = 0; i < 4; i++){
        cvsrc[i].x = src[i].x;
        cvsrc[i].y = src[i].y;
        cvdst[i].x = dst[i].x;
        cvdst[i].y = dst[i].y;
    }
    
    cvWarpPerspectiveQMatrix(cvsrc, cvdst, translate);  // calculate homography
    cvWarpPerspectiveQMatrix(cvdst, cvsrc, itranslate);  // calculate homography
    
    
    
}


ofVec2f coordWarping::transform(ofVec2f p){
    return transform(p.x,p.y);
}
ofVec2f coordWarping::inversetransform(ofVec2f p){
    return inversetransform(p.x,p.y);
}

//---------------------------
ofVec2f coordWarping::transform(float xIn, float yIn){
    ofVec2f out;
    float *data = translate->data.fl;
    
    float a = data[0];
    float b = data[1];
    float c = data[2];
    float d = data[3];
    
    float e = data[4];
    float f = data[5];
    float i = data[6];
    float j = data[7];
    
    //from Myler & Weeks - so fingers crossed!
    out.x = ((a*xIn + b*yIn + c) / (i*xIn + j*yIn + 1));
    out.y = ((d*xIn + e*yIn + f) / (i*xIn + j*yIn + 1));
    
    return out;
    delete data;
}

ofVec2f coordWarping::inversetransform(float xIn, float yIn){
    ofVec2f out;
    float *data = itranslate->data.fl;
    
    float a = data[0];
    float b = data[1];
    float c = data[2];
    float d = data[3];
    
    float e = data[4];
    float f = data[5];
    float i = data[6];
    float j = data[7];
    
    //from Myler & Weeks - so fingers crossed!
    out.x = ((a*xIn + b*yIn + c) / (i*xIn + j*yIn + 1));
    out.y = ((d*xIn + e*yIn + f) / (i*xIn + j*yIn + 1));
    
    return out;
    delete data;
}