#include "testApp.h"
#include "ofAppGlutWindow.h"

int main() {
	ofAppGlutWindow window;
	ofSetupOpenGL(&window, 300, 70, OF_WINDOW);
	ofRunApp(new testApp());
}
