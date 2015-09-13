#include "ofMain.h"
#include "ofxCv.h"

using namespace cv;

class ofApp : public ofBaseApp {
public:
    ofFloatImage camConfidence;
    ofFloatImage proConfidence;
    ofShortImage proMap;
    void setup() {
        ofBackground(0);
        ofSetLineWidth(2);
    }
    void draw() {
        if(camConfidence.isAllocated()) {
            float totalHeight = camConfidence.getHeight() + proConfidence.getHeight();
            float scale = ofGetHeight() / totalHeight;
            ofScale(scale, scale);
            ofVec2f proOffset(0, proConfidence.getHeight());
            ofSetColor(ofColor::white);
            proConfidence.draw(0, 0);
            
            ofPushMatrix();
            ofTranslate(proOffset);
            camConfidence.draw(0, 0);
            ofPopMatrix();
            
            float r = 2;
            float drawRadius = 64;
            ofFill();
            ofVec2f pro = ofVec2f(mouseX, mouseY) / scale;
            ofSeedRandom(0);
            for(int y = -r; y <= +r; y++) {
                for(int x = -r; x <= +r; x++) {
                    ofVec2f cur = pro + ofVec2f(x, y);
                    ofSetColor(ofColor::red);
                    ofDrawRectangle(cur.x, cur.y, 1, 1);
                    ofShortColor camColor = proMap.getColor(cur.x, cur.y);
                    ofVec2f cam(camColor.b, camColor.g);
                    ofSetColor(ofColor::fromHsb(ofRandom(255), 255, 255));
                    if(cam.x > 0 && cam.y > 0) {
                        ofVec2f center = proOffset + cam - ofVec2f(drawRadius, drawRadius) / 2;
                        ofPushMatrix();
                        ofTranslate(center);
                        ofRotate(ofRandom(360));
                        ofDrawLine(-drawRadius, 0, +drawRadius, 0);
                        ofDrawLine(0, -drawRadius, 0, +drawRadius);
                        ofPopMatrix();
                    }
                }
            }
        }
    }
    void loadScan(string path) {
        camConfidence.load(path + "/camConfidence.exr");
        proConfidence.load(path + "/proConfidence.exr");
        proMap.load(path + "/proMap.png");
    }
    void dragEvent(ofDragInfo dragInfo) {
        if(dragInfo.files.size() == 1) {
            loadScan(dragInfo.files[0]);
        }
    }
};
int main() {
    ofSetupOpenGL(1280, 720, OF_FULLSCREEN);
    ofRunApp(new ofApp());
}