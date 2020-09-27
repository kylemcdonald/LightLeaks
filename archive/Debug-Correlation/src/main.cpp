#include "ofMain.h"
#include "ofxCv.h"

using namespace cv;
using namespace ofxCv;

void lookupCamPosition(ofShortImage& proMap,
                       unsigned short px, unsigned short py,
                       unsigned short& cx, unsigned short& cy) {
    const Mat proMapMat = toCv(proMap);
    Vec3w camColor = proMapMat.at<Vec3w>(py, px);
    cx = camColor[0];
    cy = camColor[1];
}

class ofApp : public ofBaseApp {
public:
    int ox, oy;
    ofFloatImage camConfidence;
    ofFloatImage proConfidence;
    ofShortImage proMap;
    ofImage nonmatch;
    void setup() {
        ofBackground(0);
        ofSetFrameRate(30);
        ofSetLineWidth(2);
        loadScan("../../../SharedData/3-scan-1906-lowconf/");
        ox = 0, oy = 0;
    }
    void draw() {
        if(camConfidence.isAllocated()) {
            int height = ofGetHeight();
            int width = ofGetWidth() / 2;
            int scale = ofGetMousePressed() ? 2 : 16;
            unsigned short px = ox + ofMap(mouseX, 0, ofGetWidth(), 0, proConfidence.getWidth());
            unsigned short py = oy + ofMap(mouseY, 0, ofGetHeight(), 0, proConfidence.getHeight());
            
            ofNoFill();
            
            ofPushView();
            ofViewport(ofRectangle(0, 0, width, height));
            ofSetupScreenPerspective();
            ofPushMatrix();
            ofTranslate(width / 2, height / 2);
            ofScale(scale, scale);
            if(ofGetKeyPressed('n')) {
                nonmatch.draw(-px, -py);
            } else {
                proConfidence.draw(-px, -py);
            }
            ofDrawRectangle(0, 0, 1, 1);
            ofPopMatrix();
            ofPopView();
            
            unsigned short cx, cy;
            lookupCamPosition(proMap, px, py, cx, cy);
            
            ofPushView();
            ofViewport(ofRectangle(width, 0, width, height));
            ofSetupScreenPerspective();
            ofPushMatrix();
            ofTranslate(width / 2, height / 2);
            ofScale(scale, scale);
            camConfidence.draw(-cx, -cy);
            ofDrawRectangle(0, 0, 1, 1);
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
    void mouseMoved(int x, int y) {
        ox = 0, oy = 0;
    }
    void keyPressed(int key) {
        if(key == OF_KEY_LEFT) {
            ox--;
        }
        if(key == OF_KEY_RIGHT) {
            ox++;
        }
        if(key == OF_KEY_UP) {
            oy--;
        }
        if(key == OF_KEY_DOWN) {
            oy++;
        }
    }
    void loadScan(string path) {
        camConfidence.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
        proConfidence.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
        nonmatch.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
        camConfidence.load(path + "/camConfidence.exr");
        proConfidence.load(path + "/proConfidence.exr");
        proMap.setUseTexture(false);
        proMap.load(path + "/proMap.png");
        
        imitate(nonmatch, proMap, CV_8UC1);
        Mat pcMat = toCv(proConfidence);
        Mat ccMat = toCv(camConfidence);
        Mat pmMat = toCv(proMap);
        Mat nmMat = toCv(nonmatch);
        int rows = pcMat.rows;
        int cols = pcMat.cols;
        for(int row = 0; row < rows; row++) {
            for(int col = 0; col < cols; col++) {
                Vec3w cxy = pmMat.at<Vec3w>(row, col);
                float generatedConfidence = ccMat.at<float>(cxy[1], cxy[0]);
                float originalConfidence = pcMat.at<float>(row, col);
                nmMat.at<unsigned char>(row, col) = 255 * fabsf(generatedConfidence - originalConfidence);
            }
        }
        nonmatch.update();
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