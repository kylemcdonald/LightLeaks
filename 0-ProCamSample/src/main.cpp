#include "ofMain.h"
#include "ofAppGLFWWindow.h"
#include "EdsdkOsc.h"
#include "GrayCodeGenerator.h"

int totalPhysicalProjectors, totalProjectors, tw, th;

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
        ofLog() << "Creating " << curDirectory;
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
		if(ofFile::doesFileExist("../../../SharedData/mask.png")) {
			mask.load("../../../SharedData/mask.png");
            ofLog() << "Loaded mask";
            ofEnableBlendMode(OF_BLENDMODE_MULTIPLY);
            ofBackground(255);
        } else {
            ofEnableAlphaBlending();
            ofBackground(0);
        }
        
        ofSetWindowPosition(1680,0);
        ofSetWindowShape(1920*3, 1200);
		ofSetLogLevel(OF_LOG_VERBOSE);
        camera.setup();
	}
	
	void update() {
		if(!capturing) {
			camera.update();
            
            if(camera.start){
                camera.start = false;
                
                pattern = 0;
                projector = 0;
                direction = 0;
                inverse = 0;
                
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                    camera.takePhoto(curDirectory + ofToString(pattern) + ".jpg");
                });
                
                timestamp = ofToString(ofGetHours())+ofToString(ofGetMinutes());
                generate();

                capturing = true;
            }
		}
		if(camera.isPhotoNew()) {
            if(camera.error){
                captureTime = ofGetElapsedTimeMillis();
                needToCapture = true;
            } else {
                if(nextState()) {
                    captureTime = ofGetElapsedTimeMillis();
                    needToCapture = true;
                } else {
                    cout<<"Done taking photos. Huray!"<<endl;
                    capturing = false;
                }
            }
            
		}
	}
	
	void draw() {
		ofSetColor(255);
       
		if(!capturing) {

			/*ofPushMatrix();
			ofScale(.25, .25);
			camera.draw(0, 0);
			ofPopMatrix();*/
            
            int singleWidth = tw / totalPhysicalProjectors;
            for(int i = 0; i < totalPhysicalProjectors; i++) {
                switch(i % 3) {
                    case 0: ofSetColor(255,0,0); break;
                    case 1: ofSetColor(0,255,0); break;
                    case 2: ofSetColor(0,0,255); break;
                }
                ofDrawRectangle(i * singleWidth, 0, singleWidth, th);
            }
        } else {
            if(generated){
                generator.get(pattern).draw(projector * tw, 0);
            }
            if(mask.getWidth() > 0) {
                mask.draw(0, 0);
            }
        }
		if(needToCapture && (ofGetElapsedTimeMillis() - captureTime) > bufferTime) {
			camera.takePhoto(curDirectory + ofToString(pattern) + ".jpg");
			needToCapture = false;
		}
	}
	
	void keyPressed(int key) {
		if(key == OF_KEY_RIGHT) {
			pattern++;
		}
        if(key == OF_KEY_LEFT) {
			pattern--;
		}
		if(key == ' ') {
            timestamp = ofToString(ofGetHours())+ofToString(ofGetMinutes());
            generate();
            
			camera.takePhoto(curDirectory + ofToString(pattern) + ".jpg");
			capturing = true;
		}
		if(key == 'r') {
			camera.takePhoto(curDirectory + ofToString(pattern) + ".jpg");
			referenceImage = true;
		}
		if(key == 'f') {
			ofToggleFullscreen();
		}
	}
};

int main() {
	ofXml settings;
	settings.load("../../../SharedData/settings.xml");
	totalProjectors = settings.getIntValue("projectors/count");
    totalPhysicalProjectors = totalProjectors;
	tw = settings.getIntValue("projectors/width");
	th = settings.getIntValue("projectors/height");
    
    if(settings.getBoolValue("sample/singlepass")) {
        ofLog() << "Using single pass sampling";
        tw *= totalProjectors;
        totalProjectors = 1;
    }
	
    ofAppGLFWWindow win;
    
    win.setMultiDisplayFullscreen(true); //this makes the fullscreen window span across all your monitors
    
    ofSetupOpenGL(&win, 1280, 720, OF_FULLSCREEN);
	//ofSetupOpenGL(&win, totalProjectors * tw, th, OF_FULLSCREEN);
	ofRunApp(new testApp());
}
