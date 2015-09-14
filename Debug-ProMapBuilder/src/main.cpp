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

bool inside(const unsigned short& x,
            const unsigned short& y,
            const int& w,
            const int& h) {
    return x >= 0 && x < w && y >= 0 && y < h;
}

void buildProMapLerp(const Mat& binaryCoded,
                     const Mat& camConfidence,
                     Mat& proConfidence,
                     Mat& proMap) {
    proConfidence.setTo(0);
    proMap.setTo(0);
    
    int ph = proConfidence.rows;
    int pw = proConfidence.cols;
    int ch = camConfidence.rows;
    int cw = camConfidence.cols;
    for(int cy = 0; cy < ch-1; cy++) {
        for(int cx = 0; cx < cw-1; cx++) {
            const float& c00conf = camConfidence.at<float>(cy, cx);
            const Vec3w& c00map = binaryCoded.at<Vec3w>(cy, cx);
            const unsigned short& p00x = c00map[0], p00y = c00map[1];
            
            const float& c10conf = camConfidence.at<float>(cy, cx+1);
            const Vec3w& c10map = binaryCoded.at<Vec3w>(cy, cx+1);
            const unsigned short& p10x = c10map[0], p10y = c10map[1];
            
            const float& c01conf = camConfidence.at<float>(cy+1, cx);
            const Vec3w& c01map = binaryCoded.at<Vec3w>(cy+1, cx);
            const unsigned short& p01x = c01map[0], p01y = c01map[1];
            
            if(inside(p00x, p00y, pw, ph)) {
                if(inside(p10x, p10y, pw, ph)) {
                    short dx = (short) p00x - (short) p10x;
                    short dy = (short) p00y - (short) p10y;
                }
                if(inside(p01x, p01y, pw, ph)) {
                    short dx = (short) p00x - (short) p01x;
                    short dy = (short) p00y - (short) p01y;
                }
            }
        }
    }
}

void medianThreshold(cv::Mat& src, float thresholdValue) {
    cv::Mat thresholded, filtered;
    ofxCv::threshold(src, thresholded, thresholdValue);
    ofxCv::medianBlur(thresholded, filtered, 3);
    thresholded &= filtered;
    min(src, thresholded, src);
}

void buildProMapDist(const Mat& binaryCodedIn,
                     const Mat& camConfidenceIn,
                     Mat& proConfidence,
                     Mat& proMap,
                     int k) {
    Mat camConfidence;
    Mat binaryCoded;
    cv::blur(camConfidenceIn, camConfidence, cv::Size(k, k));
    cv::blur(binaryCodedIn, binaryCoded, cv::Size(k, k));
    
    proConfidence.setTo(0);
    proMap.setTo(0);
    int n = 1 + k * 2;
    Mat1f weights = Mat1f::zeros(n, n);
    for(int y = 0; y < n; y++) {
        for(int x = 0; x < n; x++) {
            float dx = x - k, dy = y - k;
            float distance = sqrtf(dx * dx + dy * dy);
            weights(y, x) = (float) k / (k + distance); // falloff function
        }
    }
    int ph = proConfidence.rows;
    int pw = proConfidence.cols;
    int ch = camConfidence.rows;
    int cw = camConfidence.cols;
    for(int cy = 0; cy < ch; cy++) {
        for(int cx = 0; cx < cw; cx++) {
            const float& cconf = camConfidence.at<float>(cy, cx);
            const Vec3w& cmap = binaryCoded.at<Vec3w>(cy, cx);
            int px = cmap[0], py = cmap[1];
            if(px >= 0 && px < pw && py >= 0 && py < ph) {
                for(int oy = -k; oy <= +k; oy++) {
                    for(int ox = -k; ox <= +k; ox++) {
                        int pox = px + ox, poy = py + oy;
                        if(pox >= 0 && pox < pw && poy >= 0 && poy < ph) {
                            int wx = ox + k, wy = oy + k;
                            float& weight = weights(wy, wx);
                            float& pconf = proConfidence.at<float>(poy, pox);
                            float curconf = weight * cconf;
                            if(curconf > pconf) {
                                Vec3w& pmap = proMap.at<Vec3w>(poy, pox);
                                pmap(0) = cx;
                                pmap(1) = cy;
                                pconf = curconf;
                            }
                        }
                    }
                }
            }
        }
    }
    medianThreshold(proConfidence, .25);
}

void buildProMapBlur(const Mat& binaryCodedIn,
                     const Mat& camConfidenceIn,
                     Mat& proConfidence,
                     Mat& proMap,
                     int k) {
    int ph = proConfidence.rows;
    int pw = proConfidence.cols;
    int ch = camConfidenceIn.rows;
    int cw = camConfidenceIn.cols;
    proConfidence.setTo(0);
    proMap.setTo(0);
    
    Mat camConfidence;
    Mat binaryCoded;
    cv::blur(camConfidenceIn, camConfidence, cv::Size(k, k));
    cv::blur(binaryCodedIn, binaryCoded, cv::Size(k, k));
    
    for(int cy = 0; cy < ch; cy++) {
        for(int cx = 0; cx < cw; cx++) {
            float curCamConfidence = camConfidence.at<float>(cy, cx);
            const Vec3w& pxy = binaryCoded.at<Vec3w>(cy, cx);
            const unsigned short& px = pxy[0], py = pxy[1];
            if(px < pw && py < ph) {
                Vec3w curProMap(cx, cy, 0);
                float& curProConfidence = proConfidence.at<float>(py, px);
                if(curCamConfidence > curProConfidence) {
                    curProConfidence = curCamConfidence;
                    proMap.at<Vec3w>(py, px) = curProMap;
                }
            }
        }
    }
    medianThreshold(proConfidence, .25);
}

class ofApp : public ofBaseApp {
public:
    int ox, oy;
    ofShortImage binaryCoded, proMap;
    ofFloatImage camConfidence, proConfidence;
    void setup() {
        ofBackground(0);
        ofSetFrameRate(30);
        ofSetLineWidth(2);
        loadScan("../../../SharedData/_kyle");
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
            proConfidence.draw(-px, -py);
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
        camConfidence.load(path + "/camConfidence.exr");
        binaryCoded.load(path + "/binaryCoded.png");
        
        int proWidth = 1920 * 3;
        int proHeight = 1200;
        proConfidence.allocate(proWidth, proHeight, OF_IMAGE_GRAYSCALE);
        proMap.allocate(proWidth, proHeight, OF_IMAGE_COLOR);
        Mat proConfidenceMat = toCv(proConfidence);
        Mat proMapMat = toCv(proMap);
        buildProMapDist(toCv(binaryCoded), toCv(camConfidence),
                        proConfidenceMat, proMapMat, 3);
        proConfidence.update();
        proMap.update();
        proConfidence.save("proConfidence.exr");
        proMap.save("proMap.png");
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