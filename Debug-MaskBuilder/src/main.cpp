#include "ofMain.h"
#include "ofAppGLFWWindow.h"

int sw = 1920*2, sh = 1200, sc = 1;

class ofApp : public ofBaseApp {
public:
	ofFbo fbo;
	ofVec2f mouseCur, mousePrev;
	float radius;
	int screen;
	
	void setup() {
		radius = 10;
		screen = 0;
		fbo.allocate(sw, sh);
		fbo.begin();
		ofClear(0, 255);
		fbo.end();
		ofHideCursor();
        
        ofSetWindowPosition(sh,0);
        ofSetWindowShape(sw, sh);

	}
	void update() {
		fbo.begin();
        ofPushStyle();
		if(ofGetKeyPressed()) {
			ofSetColor(0);
		} else {
			ofSetColor(255);
		}
		if(ofGetMousePressed()) {
			float length = mousePrev.distance(mouseCur);
			for(int i = 0; i < length; i++) {
				ofDrawCircle(mousePrev.getInterpolated(mouseCur, i / length), radius);
			}
			mousePrev = mouseCur;
		}
		ofPopStyle();
		fbo.end();
	}
	void draw() {
		ofBackground(0);
		ofTranslate(sw * screen, 0);
		fbo.draw(0, 0);
		ofPushStyle();
		ofNoFill();
		ofSetColor(255);
		ofDrawCircle(mouseCur, radius);
		ofFill();
		ofSetColor(255, 64);
		ofDrawCircle(mouseCur, radius);
		ofPopStyle();
	}
	void mouseMoved(int x, int y) {
		mouseCur.set(x - sw * screen, y);
	}
	void mouseDragged(int x, int y, int b) {
		mouseCur.set(x - sw * screen, y);
	}
	void mousePressed(int x, int y, int b) {
		mousePrev.set(x - sw * screen, y);
	}
	void keyPressed(int key) {
		if(key == '=') {
			radius++;
		}
		if(key == '-') {
			radius--;
		}
		if(key == 'f') {
			ofToggleFullscreen();
		}
		if(key == '\t') {
			screen = (screen + 1) % sc;
		}
	}
	void mouseReleased(int x, int y, int b) {
		ofPixels pix;
		fbo.readToPixels(pix);
		ofSaveImage(pix, "mask-" + ofToString(screen) + "-" + ofGetTimestampString() + ".png");
	}
};

int main() {
//	ofSetupOpenGL(1280, 720, OF_WINDOW);
    ofAppGLFWWindow win;
    win.setMultiDisplayFullscreen(true); //this makes the fullscreen window span across all your monitors
    ofSetupOpenGL(&win, 1280, 720, OF_FULLSCREEN);

	ofRunApp(new ofApp());
}