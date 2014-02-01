#include "ofMain.h"
#include "ofAutoShader.h"
#include "ofxAssimpModelLoader.h"

class ofApp : public ofBaseApp {
public:
	ofImage front, back, sides, floor;
	ofImage ao;
	ofEasyCam cam;
	ofAutoShader shader;
	ofxAssimpModelLoader model;
	
	void setup() {
		shader.setup("shader/shader");
		front.loadImage("reflections/frontMap.png");
		back.loadImage("reflections/backMap.png");
		sides.loadImage("reflections/sideMap.png");
		floor.loadImage("reflections/floorMap.png");
		ao.loadImage("ao.png");
		model.loadModel("mesh.stl");
	}
	void update() {
		
	}
	void drawWithShader(ofImage& img) {
		shader.setUniformTexture("tex", img, 0);
		shader.setUniform2f("size", img.getWidth(), img.getHeight());
		img.draw(0, 0);
	}
	void draw() {
		ofBackgroundGradient(64, 0);
		
		cam.begin();
		
		ofSetColor(255);
		shader.begin();
		shader.setUniform1f("elapsedTime", ofGetElapsedTimef());
		shader.setUniform2f("mouse", mouseX, mouseY);
		shader.setUniform1f("baseBrightness", .3);
		shader.setUniformTexture("ao", ao, 1);
		
		ofPushMatrix();
		ofTranslate(-front.getWidth() / 2, -front.getHeight() / 2, +sides.getWidth() / 2);
		
		shader.setUniform1f("diffuse", 0);
		
		ofPushMatrix();
		ofRotateY(90);
		drawWithShader(sides);
		ofTranslate(0, 0, front.getWidth());
		drawWithShader(sides);
		ofPopMatrix();
		
		ofPushMatrix();
		ofTranslate(0, 0, -sides.getWidth());
		drawWithShader(back);
		ofPopMatrix();
		
		ofPushMatrix();
		ofRotateX(-90);
		drawWithShader(floor);
		ofPopMatrix();
		
		shader.setUniform1f("diffuse", 1);
		drawWithShader(front);
		
		shader.end();
		ofPopMatrix();
		
		ofSetColor(128, 128);
		ofRotateX(90);
		float scaleFactor = 1.5;
		ofScale(scaleFactor, scaleFactor, scaleFactor);
		ofTranslate(0, 0, -208);
		model.drawFaces();
		
		cam.end();
	}
};

int main() {
	ofSetupOpenGL(1280, 720, OF_WINDOW);
	ofRunApp(new ofApp());
}