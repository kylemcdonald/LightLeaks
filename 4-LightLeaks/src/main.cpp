#include "ofApp.h"
#include "ofAppGLFWWindow.h"

int main() {
    ofAppGLFWWindow win;

    win.setMultiDisplayFullscreen(true); //this makes the fullscreen window span across all your monitors

    ofSetupOpenGL(&win, 1280, 720, OF_FULLSCREEN);
	ofRunApp(new ofApp());
}
