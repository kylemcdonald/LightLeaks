#include "ofMain.h"
#include "ofAutoShader.h"
#include "ofAppGlutWindow.h"

class ofApp : public ofBaseApp {
public:
	ofAutoShader shader;
	ofFbo fbo;
	ofImage mask0, mask1;
	
	ofSoundPlayer drone;
	vector<ofPtr<ofSoundPlayer> > sounds;
	int curSound = 0;
	
	int lastBaseStage = -1;
	
	void setup() {
		ofSetVerticalSync(false);
		ofSetFrameRate(120);
		shader.setup("shader");
		mask0.loadImage("mask0.png");
		mask1.loadImage("mask1.png");
		fbo.allocate(1920, 1080);
		ofHideCursor();
		
		ofDirectory dir;
		dir.listDir("sound");
		for(int i = 0; i < dir.size(); i++) {
			string cur = dir.getPath(i);
			if(cur == "sound/NAPPE.wav") {
				ofLog() << "Loading drone " << cur;
				drone.loadSound(cur);
				drone.setLoop(true);
				drone.play();
			} else {
				ofLog() << "Loading sound " << cur;
				sounds.push_back(ofPtr<ofSoundPlayer>(new ofSoundPlayer()));
				sounds.back()->loadSound(cur);
				sounds.back()->setLoop(false);
			}
		}
	}
	
	float fract(float x) {
		return fmodf(x, 1);
	}
	
	float mod(float x, float y) {
		return fmodf(x, y);
	}
	
	float rand() {
		return ofRandom(1);
		//return fract(sin(seed * 12.9898) * 43758.5453);
	}
	
	float window(float x) {
		return pow(.5 - cos(TWO_PI * x) * .5, .1);
	}
	
	float smoothStep(float x, float stepSize) {
		float wavelength = PI / stepSize;
		return x + sin(x * wavelength) / wavelength;
	}
	
	void draw() {
		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
		ofBackground(255);
		fbo.begin();
		shader.begin();

		float progress, fade;
		float stage;
		float positionDivider;
		float scrollSpeed;
		float offsetSpeed;
		float time;
		float camoStrength;
		float bw;
		
		float stageDuration = 32;
		int stageCount = 6;
		float stepDuration = 2;
		bool useStepTime = true;
		
		unsigned long long totalVariations = 100;
		unsigned long long millisPerSecond = 1000;
		unsigned long long totalDurationMillis = totalVariations * stageCount * stageDuration * millisPerSecond;
		unsigned long long loopingTime = ofGetElapsedTimeMillis() % totalDurationMillis;
		float elapsedTime = loopingTime / (float) millisPerSecond;
			
		progress = fract(elapsedTime / stageDuration);
		fade = window(progress);
		int baseStage = elapsedTime / stageDuration;
		ofSeedRandom(baseStage);
		stage = mod(baseStage, stageCount);
		positionDivider = 256 * rand() + 128;
		offsetSpeed = 256 * rand();
		scrollSpeed = 256 * rand();
		if(offsetSpeed < 170) {
			offsetSpeed = 0;
		}
		if(scrollSpeed < 128) {
			scrollSpeed = 0;
		}
		camoStrength = rand() * 50;
		bw = rand();
		time = elapsedTime;
		if(useStepTime) {
			time = smoothStep(elapsedTime, stepDuration) + smoothStep(elapsedTime, stageDuration);
		}
		
		if(baseStage != lastBaseStage) {
			sounds[curSound]->stop();
			int nextSound = curSound;
			while(sounds.size() > 1 && nextSound == curSound) {
				nextSound = MIN(ofRandom(sounds.size()), sounds.size() - 1);
			}
			curSound = nextSound;
			sounds[curSound]->play();
		}
		lastBaseStage = baseStage;
		
		shader.setUniform2f("size", ofGetWidth(), ofGetHeight());
		shader.setUniform1f("progress", progress);
		shader.setUniform1f("fade", fade);
		shader.setUniform1f("stage", stage);
		shader.setUniform1f("positionDivider", positionDivider);
		shader.setUniform1f("scrollSpeed", scrollSpeed);
		shader.setUniform1f("offsetSpeed", offsetSpeed);
		shader.setUniform1f("time", time);
		shader.setUniform1f("camoStrength", camoStrength);
		shader.setUniform1f("bw", bw);
		ofRect(0, 0, fbo.getWidth(), fbo.getHeight());
		shader.end();
		fbo.end();
		
		ofEnableBlendMode(OF_BLENDMODE_MULTIPLY);
		fbo.draw(0, 0);
		mask0.draw(0, 0);
		fbo.draw(1920, 0);
		mask1.draw(1920, 0);
	}
	
	void keyPressed(int key) {
		if(key == 'f') {
			ofToggleFullscreen();
		}
	}
};

int main() {
	ofAppGlutWindow window;
	ofSetupOpenGL(ofPtr<ofAppBaseWindow>(new ofAppGlutWindow()), 1920 * 2, 1080, OF_FULLSCREEN);
//	ofSetupOpenGL(&window, 1280, 720, OF_WINDOW);
	ofRunApp(new ofApp());
}
