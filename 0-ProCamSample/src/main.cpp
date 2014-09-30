#include "ofMain.h"
#include "ofAppGlutWindow.h"
#include "EdsdkOsc.h"
#include "GrayCodeGenerator.h"

int totalProjectors, tw, th;

class testApp : public ofBaseApp {
public:
	ofImage mask;
	EdsdkOsc camera;
	
	GrayCodeGenerator generator;
	bool capturing = false;
	int totalDirection = 2;
	int totalInverse = 2;
	
	// every combination of these four properties
	int projector = 0; // n values = totalProjectors
	int direction = 0; // false/true, 2 values
	int inverse = 0; // false/true, 2 values
	int pattern = 0; // depends on projector resolution
	
	string curDirectory;
	long bufferTime = 100;
	bool needToCapture = false;
	long captureTime = 0;
	bool referenceImage = false;
    
    bool generated;
    string timestamp;
	
	void generate() {
		generator.setSize(tw, th);
		generator.setOrientation(direction == 0 ? PatternGenerator::VERTICAL : PatternGenerator::HORIZONTAL);
		generator.setInverse(inverse == 0);
		generator.generate();
		stringstream dirStr;
		dirStr <<
        "../../../SharedData/scan-"<< timestamp << "/cameraImages/" <<
		(direction == 0 ? "vertical/" : "horizontal/") <<
		(inverse == 0 ? "inverse/" : "normal/");
		curDirectory = dirStr.str();
        cout<<"Create "<<curDirectory<<endl;
		camera.createDirectory(curDirectory);
        generated = true;
	}
	
	bool nextState() {
		pattern++;
		if(pattern == generator.size()) {
			pattern = 0;
			inverse++;
			if(inverse == totalInverse) {
				inverse = 0;
				direction++;
				if(direction == totalDirection) {
					direction = 0;
					projector++;
					if(projector == totalProjectors) {
						projector = 0;
						return false;
					}
				}
			}
			generate();
		}
		return true;
	}
	
	void setup() {
		ofSetVerticalSync(true);
		ofHideCursor();
		if(ofFile::doesFileExist("mask.png")) {
			mask.loadImage("mask.png");
		}
		ofEnableAlphaBlending();
		ofSetLogLevel(OF_LOG_VERBOSE);
		camera.setup();
        
	}
	
	void update() {
		if(!capturing) {
			camera.update();
            
            if(camera.start){
                camera.start = false;
                camera.takePhoto();
                
                timestamp = ofToString(ofGetHours())+ofToString(ofGetMinutes());
                generate();

                capturing = true;
            }
		}
		if(camera.isPhotoNew()) {
			if(referenceImage) {
				camera.savePhoto("referenceImage.jpg");
			} else {
				camera.savePhoto(curDirectory + ofToString(pattern) + ".jpg");
				if(nextState()) {
					captureTime = ofGetElapsedTimeMillis();
					needToCapture = true;
				} else {
					capturing = false;
				}
			}
		}
	}
	
	void draw() {
		ofBackground(0);
		ofSetColor(255);
        if(generated){
            generator.get(pattern).draw(projector * tw, 0);
        }
		if(mask.getWidth() > 0) {
			mask.draw(0, 0);
		}
		if(!capturing) {
			ofPushMatrix();
			ofScale(.25, .25);
			camera.draw(0, 0);
			ofPopMatrix();
		}
		if(needToCapture && (ofGetElapsedTimeMillis() - captureTime) > bufferTime) {
			camera.takePhoto();
			needToCapture = false;
		}
	}
	
	void keyPressed(int key) {
		if(key == ']') {
			pattern++;
		}
		if(key == '[') {
			pattern--;
		}
		if(key == ' ') {
            timestamp = ofToString(ofGetHours())+ofToString(ofGetMinutes());
            generate();
            
			camera.takePhoto();
			capturing = true;
		}
		if(key == 'r') {
			camera.takePhoto();
			referenceImage = true;
		}
		if(key == 'f') {
			ofToggleFullscreen();
		}
	}
};

int main() {
	ofSetWorkingDirectoryToDefault();
	ofXml settings;
	settings.load("../../../SharedData/settings.xml");
	totalProjectors = settings.getIntValue("projectors/count");
	tw = settings.getIntValue("projectors/width");
	th = settings.getIntValue("projectors/height");
	
	ofAppGlutWindow window;
	ofSetupOpenGL(&window, totalProjectors * tw, th, OF_FULLSCREEN);
	ofRunApp(new testApp());
}
