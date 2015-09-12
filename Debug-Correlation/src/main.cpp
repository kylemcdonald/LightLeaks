#include "ofMain.h"
#include "ofxCv.h"

using namespace cv;

class ofApp : public ofBaseApp {
public:
    ofImage referenceImage;
    ofImage proConfidence;
    ofImage proMap;
    void setup() {
    }
    void update() {
    }
    void draw() {
    }
    void loadScan(string path) {
//        referenceImage.load(
    }
    void dragEvent(ofDragInfo dragInfo) {
        if(dragInfo.files.size() == 1) {
            loadScan(dragInfo.files[0]);
        }
    }
};
int main() {
    ofSetupOpenGL(1280, 720, OF_WINDOW);
    ofRunApp(new ofApp());
}