#include "ofApp.h"
#include "LightLeaksUtilities.h"

using namespace ofxCv;
using namespace cv;

ofVec3f minCorner(-263.163, -272.677, 0), maxCorner(212.703, 433.583, 148.465);
float range = 706.26;

void ofApp::setup() {
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	
    
    vector<ofFile> scanNames = getScanNames();
    cout<<scanNames.size()<<endl;
    for(int i=0;i<scanNames.size();i++){
        mesh.push_back(ofVboMesh());
        drawMesh.push_back(true);
        string path = scanNames[i].path()+"/";
        if(path[0] != '-') {
            ofLogVerbose() << "processing " << path;
            directories.push_back(scanNames[i].getFileName());
            
            ofLoadImage(proMap, path + "/proMap.png");
            ofLoadImage(xyzMap, path + "/xyzMap.exr");
            ofLoadImage(proConfidence, path + "/proConfidence.exr");
            ofLoadImage(maskImage, path + "/mask.png");
            
            ofFloatColor color = ofColor::fromHsb((255 * i) / scanNames.size(), 255, 255);
            mesh[i].setMode(OF_PRIMITIVE_POINTS);
            for(int y = 0; y < proMap.getHeight(); y++) {
                for(int x = 0; x < proMap.getWidth(); x++) {
                    ofFloatColor confidence = proConfidence.getColor(x, y);
                    if(confidence.r > .25) {
                        ofShortColor pxy = proMap.getColor(x, y);
                        int cx = pxy.r, cy = pxy.g;
                        ofFloatColor position = xyzMap.getColor(cx / 4, cy / 4);
                        int mask = maskImage.getColor(cx , cy).r;
                        if(mask > 100){
                            mesh[i].addVertex(ofVec3f(position.r, position.g, position.b));
                            mesh[i].addColor(color);
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
    for(int i=0;i<mesh.size();i++){
        if(drawMesh[i]){
            mesh[i].draw();
        }
    }
	cam.end();
	
	for(int i = 0; i < directories.size(); i++) {
        if(drawMesh[i]){
            ofSetColor(ofColor::fromHsb((255 * i) / directories.size(), 255, 255));
        } else {
            ofSetColor(ofColor::fromHsb((255 * i) / directories.size(), 255, 100));
        }

		ofDrawBitmapString(directories[i], 10, i * 20 + 10);
	}
}

void ofApp::keyPressed(int key){
    int n = key-'1';
    if(n >= 0 && n <mesh.size()){
        drawMesh[n] = ! drawMesh[n];
    }
}