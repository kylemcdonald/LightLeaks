#include "ofApp.h"
#include "LightLeaksUtilities.h"

using namespace ofxCv;
using namespace cv;

ofVec3f minCorner(-263.163, -272.677, 0), maxCorner(212.703, 433.583, 148.465);
float range = 706.26;

void ofApp::setup() {
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
    
    
    setCalibrationDataPathRoot();
    ofSetLogLevel(OF_LOG_VERBOSE);
	
    
    vector<ofFile> scanNames = getScanNames();
    cout<<scanNames.size()<<endl;
    for(int i=0;i<scanNames.size();i++){

        string path = scanNames[i].path()+"/";
        if(path[0] != '-') {
            ofLogVerbose() << "processing " << path;
            directories.push_back(scanNames[i].getFileName());
            
            ofLoadImage(proMap, path + "/proMap.png");
            ofLoadImage(xyzMap, path + "/xyzMap.exr");
            ofLoadImage(proConfidence, path + "/proConfidence.exr");
            ofLoadImage(maskImage, path + "/mask.png");
            
            ofFloatColor color = ofColor::fromHsb((255 * i) / scanNames.size(), 255, 255);
            mesh.setMode(OF_PRIMITIVE_POINTS);
            for(int y = 0; y < proMap.getHeight(); y++) {
                for(int x = 0; x < proMap.getWidth(); x++) {
                    ofFloatColor confidence = proConfidence.getColor(x, y);
                    if(confidence.r > .3) {
                        ofShortColor pxy = proMap.getColor(x, y);
                        int cx = pxy.r, cy = pxy.g;
                        ofFloatColor position = xyzMap.getColor(cx / 4, cy / 4);
                        int mask = maskImage.getColor(cx / 4, cy / 4).r;
                        if(mask > 100){
                            mesh.addVertex(ofVec3f(position.r, position.g, position.b));
                            mesh.addColor(color);
                        }
                    }
                }
            }
        }
    }
    
}


void ofApp::update() {
}

void ofApp::draw() {
	ofBackground(0);
	cam.begin();
	ofTranslate(-maxCorner);
	ofScale(range, range, range);
	mesh.draw();
	cam.end();
	
	for(int i = 0; i < directories.size(); i++) {
		ofSetColor(ofColor::fromHsb((255 * i) / directories.size(), 255, 255));
		ofDrawBitmapString(directories[i], 10, i * 20 + 10);
	}
}