#include "ofApp.h"

#include "LightLeaksUtilities.h"

using namespace cv;
using namespace ofxCv;

template <class T>
void removeIslands(ofPixels_<T>& img) {
	int w = img.getWidth(), h = img.getHeight();
	int ia1=-w-1,ia2=-w-0,ia3=-w+1,ib1=-0-1,ib3=-0+1,ic1=+w-1,ic2=+w-0,ic3=+w+1;
	T* p = img.getPixels();
	for(int y = 1; y + 1 < h; y++) {
		for(int x = 1; x + 1 < w; x++) {
			int i = y * w + x;
			if(p[i]) {
				if(!p[i+ia1]&&!p[i+ia2]&&!p[i+ia3]&&!p[i+ib1]&&!p[i+ib3]&&!p[i+ic1]&&!p[i+ic2]&&!p[i+ic3]) {
					p[i] = 0;
				}
			}
		}
	}
}

void ofApp::setup() {
	ofSetLogLevel(OF_LOG_VERBOSE);
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	
	float threshold = 0.0;
	int scaleFactor = 4;
	
	setCalibrationDataPathRoot();
	
	vector<ofFile> scanNames = getScanNames();
	for(int i = 0; i < scanNames.size(); i++) {
		ofFile scanName = scanNames[i];
		string path = scanName.path();
		if(scanName.isDirectory() && path[0] != '_') {
			ofLogVerbose() << "processing " << path;
			ofFloatImage xyzMap, proConfidence, normalMap;
			ofShortImage proMap;
			
			xyzMap.loadImage(path + "/xyzMap.exr");
			normalMap.loadImage(path + "/normalMap.exr");
			proConfidence.loadImage(path + "/proConfidence.exr");
			proMap.loadImage(path + "/proMap.png");
			
			Mat xyzMapMat = toCv(xyzMap);
			Mat normalMapMat = toCv(normalMap);
			Mat proConfidenceMat = toCv(proConfidence);
			Mat proMapMat = toCv(proMap);
			
			if(proXyzCombined.cols == 0) {
				proXyzCombined = Mat::zeros(proMapMat.rows, proMapMat.cols, CV_32FC4);
				proNormalCombined = Mat::zeros(proMapMat.rows, proMapMat.cols, CV_32FC4);
				proConfidenceCombined = Mat::zeros(proConfidenceMat.rows, proConfidenceMat.cols, CV_32FC1);
			}
			
			int w = proXyzCombined.cols, h = proXyzCombined.rows;
			for(int y = 0; y < h; y++) {
				for(int x = 0; x < w; x++) {
					float cur = proConfidenceMat.at<float>(y, x);
					if(cur > proConfidenceCombined.at<float>(y, x) && cur > threshold) {
						proConfidenceCombined.at<float>(y, x) = proConfidenceMat.at<float>(y, x);
						Vec3w cur = proMapMat.at<Vec3w>(y, x);
						proXyzCombined.at<Vec4f>(y, x) = xyzMapMat.at<Vec4f>(cur[1] / scaleFactor, cur[0] / scaleFactor);
						proNormalCombined.at<Vec4f>(y, x) = normalMapMat.at<Vec4f>(cur[1] / scaleFactor, cur[0] / scaleFactor);
					}
				}
			}
		}
	}
	
	ofLogVerbose() << "saving results";
	ofFloatPixels proMapFinal, proNormalFinal, proConfidenceFinal;
	toOf(proXyzCombined, proMapFinal);
	toOf(proNormalCombined, proNormalFinal);
	toOf(proConfidenceCombined, proConfidenceFinal);
	
	removeIslands(proConfidenceFinal);
	ofSaveImage(proConfidenceFinal, "confidenceMap.exr");
	
	ofxCv::threshold(proConfidenceFinal, 0);
	int w = proXyzCombined.cols, h = proXyzCombined.rows;
	for(int y = 0; y < h; y++) {
		for(int x = 0; x < w; x++) {
			if(!proConfidenceCombined.at<float>(y, x)) {
				proXyzCombined.at<Vec4f>(y, x) = Vec4f(0, 0, 0, 0);
				proNormalCombined.at<Vec4f>(y, x) = Vec4f(0, 0, 0, 0);
			}
		}
	}
	ofSaveImage(proMapFinal, "xyzMap.exr");
	ofSaveImage(proNormalFinal, "normalMap.exr");
}

void ofApp::update() {
	
}

void ofApp::draw() {
	ofBackground(0);
}
