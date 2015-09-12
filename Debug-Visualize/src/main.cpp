#include "ofMain.h"
#include "LightLeaksUtilities.h"
#include "ofAutoShader.h"

class ofApp : public ofBaseApp {
public:
    ofEasyCam cam;
    ofVboMesh mesh;
    
    ofFloatImage xyzMap, normalMap, confidenceMap;
    ofAutoShader shader;
    int substage;
    
	void setup() {
        ofSetWindowPosition(0, 0);
        ofSetWindowShape(1280*2, 720*2);
        
        ofSetFrameRate(80);
        ofSetVerticalSync(true);
        
        setCalibrationDataPathRoot();
        shader.setup("_shaders/shader");
        
        xyzMap.load("xyzMap.exr");
        normalMap.load("normalMap.exr");
        confidenceMap.load("confidenceMap.exr");
        
        xyzMap.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
        normalMap.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
        confidenceMap.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
        
        substage = 0;
        int w = xyzMap.getWidth();
        int h = xyzMap.getHeight();
        int c = xyzMap.getPixels().getNumChannels();
        float* pixels = xyzMap.getPixels().getData();
        float* confidence = confidenceMap.getPixels().getData();
        int n = w * h * c;
        int i = 0;
        mesh.setMode(OF_PRIMITIVE_POINTS);
        for(int y = 0; y < h; y++) {
            for(int x = 0; x < w; x++) {
                if(confidence[i] > .1) {
                    float px = pixels[i + 0];
                    float py = pixels[i + 1];
                    float pz = pixels[i + 2];
                    mesh.addVertex(ofVec3f(px, py, pz));
                    mesh.addTexCoord(ofVec2f(x+.5, y+.5));
                }
                i += c;
            }
        }
    }
    void draw() {
        ofBackground(0);
        cam.setFov(90);
        
        float movementAmount = 500;
        float orientationAmount = 30;
        float t = ofGetElapsedTimef();
        float pt = t * .2, ot = t * .1;
        cam.setPosition(movementAmount * ofSignedNoise(pt, 0, 0),0,
//                        movementAmount * ofSignedNoise(0, pt, 0),
                        movementAmount * ofSignedNoise(0, 0, pt));
        cam.setOrientation(ofVec3f(orientationAmount * ofSignedNoise(ot, 0, ot),
                                   orientationAmount * ofSignedNoise(0, ot, ot),
                                   0));
        cam.begin();
        float s = 4 * ofGetWidth();
        ofScale(s, s, s);
        ofRotateX(-90);
        ofRotateZ(+60);
        ofTranslate(-.5, -.25, -.1);
//        ofTranslate(-.25, -.25, -.25);
        glPointSize(4);
        ofEnableBlendMode(OF_BLENDMODE_ALPHA);
        
        shader.begin();
        shader.setUniformTexture("xyzMap", xyzMap, 1);
        shader.setUniformTexture("normalMap", normalMap, 2);
        shader.setUniformTexture("confidenceMap", confidenceMap, 3);
        shader.setUniform1f("elapsedTime", ofGetElapsedTimef());
        shader.setUniform1i("substage", substage);
        shader.setUniform2f("mouse",
                            ofMap(mouseX, 0, ofGetWidth(), 0, 1),
                            ofMap(mouseY, 0, ofGetHeight(), 0, 1));
        mesh.draw();
        shader.end();
        cam.end();
    }
    void keyPressed(int key) {
        if(key == 'f') {
            ofToggleFullscreen();
        }
        if(key >= '0' && key <= '9') {
            substage = (key - '0');
        }
    }
};

int main() {
	ofSetupOpenGL(1280, 720, OF_WINDOW);
	ofRunApp(new ofApp());
}
