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
        ofSetFrameRate(30);
        ofSetLineWidth(2);
        loadScan("../../../SharedData/scan-1549/");
    }
    void draw() {
        if(camConfidence.isAllocated()) {
            int height = ofGetHeight();
            int width = ofGetWidth() / 2;
            int scale = ofGetMousePressed() ? 2 : 16;
            float cx = ofMap(mouseX, 0, ofGetWidth(), 0, proConfidence.getWidth());
            float cy = ofMap(mouseY, 0, ofGetHeight(), 0, proConfidence.getHeight());
            
            ofPushView();
            ofViewport(ofRectangle(0, 0, width, height));
            ofSetupScreenPerspective();
            ofPushMatrix();
            ofTranslate(width / 2, height / 2);
            ofScale(scale, scale);
            ofDrawCircle(0, 0, 4);
            ofPushMatrix();
            ofTranslate(-cx, -cy);
            proConfidence.draw(0, 0);
            ofPopMatrix();
            ofDrawRectangle(0, 0, 1, 1);
            ofPopMatrix();
            ofPopView();
            
            ofPushView();
            ofViewport(ofRectangle(width, 0, width, height));
            ofSetupScreenPerspective();
            ofPushMatrix();
            ofScale(scale, scale);
            camConfidence.draw(0, 0);
            ofPopMatrix();
            ofPopView();
            
//            float totalHeight = camConfidence.getHeight() + proConfidence.getHeight();
//            float scale = ofGetHeight() / totalHeight;
//            ofScale(scale, scale);
//            ofVec2f proOffset(0, proConfidence.getHeight());
//            ofSetColor(ofColor::white);
//            proConfidence.draw(0, 0);
//            
//            ofPushMatrix();
//            ofTranslate(proOffset);
//            camConfidence.draw(0, 0);
//            ofPopMatrix();
//            
//            float r = 2;
//            float drawRadius = 64;
//            ofFill();
//            ofVec2f pro = ofVec2f(mouseX, mouseY) / scale;
//            ofSeedRandom(0);
//            for(int y = -r; y <= +r; y++) {
//                for(int x = -r; x <= +r; x++) {
//                    ofVec2f cur = pro + ofVec2f(x, y);
//                    ofSetColor(ofColor::red);
//                    ofDrawRectangle(cur.x, cur.y, 1, 1);
//                    ofShortColor camColor = proMap.getColor(cur.x, cur.y);
//                    ofVec2f cam(camColor.b, camColor.g);
//                    ofSetColor(ofColor::fromHsb(ofRandom(255), 255, 255));
//                    if(cam.x > 0 && cam.y > 0) {
//                        ofVec2f center = proOffset + cam - ofVec2f(drawRadius, drawRadius) / 2;
//                        ofPushMatrix();
//                        ofTranslate(center);
//                        ofRotate(ofRandom(360));
//                        ofDrawLine(-drawRadius, 0, +drawRadius, 0);
//                        ofDrawLine(0, -drawRadius, 0, +drawRadius);
//                        ofPopMatrix();
//                    }
//                }
//            }
        }
    }
    void loadScan(string path) {
        camConfidence.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
        proConfidence.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
        camConfidence.load(path + "/camConfidence.exr");
        proConfidence.load(path + "/proConfidence.exr");
        proMap.setUseTexture(false);
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