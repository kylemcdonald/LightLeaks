#include "testApp.h"

using namespace ofxCv;
using namespace cv;

bool natural(const ofFile& a, const ofFile& b) {
	string aname = a.getBaseName(), bname = b.getBaseName();
	int aint = ofToInt(aname), bint = ofToInt(bname);
	if(ofToString(aint) == aname && ofToString(bint) == bname) {
		return aint < bint;
	} else {
		return a < b;
	}
}

void processGraycodeLevel(int i, int n, int dimensions, string normalPath, string inversePath, Mat cameraMask, Mat& confidence, Mat& binaryCoded) {
	ofLogVerbose() << "loading " << normalPath <<  " + " << inversePath << " " << i << " of " << n;
	ofImage imageNormal, imageInverse;
	imageNormal.loadImage(normalPath);
	imageInverse.loadImage(inversePath);
	int w = imageNormal.getWidth(), h = imageNormal.getHeight();
	cv::Mat imageNormalGray, imageInverseGray;
	convertColor(imageNormal, imageNormalGray, CV_RGB2GRAY);
	convertColor(imageInverse, imageInverseGray, CV_RGB2GRAY);
	imageNormalGray &= cameraMask;
	imageInverseGray &= cameraMask;
	float totalVariation = 0;
	for(int j = 0; j < n; j++) {
		totalVariation += 1 << (n - j - 1);
	}
	unsigned short curMask = 1 << (n - i - 1);
	float curVariation = curMask / (dimensions * 255. * totalVariation);
	for(int y = 0; y < h; y++) {
		for(int x = 0; x < w; x++) {
			unsigned char normal = imageNormalGray.at<unsigned char>(y, x);
			unsigned char inverse = imageInverseGray.at<unsigned char>(y, x);
			if(normal > inverse) {
				binaryCoded.at<unsigned short>(y, x) |= curMask;
			}
			float range = fabsf((float) normal - (float) inverse);
			confidence.at<float>(y, x) += curVariation * range;
		}
	}
}

void testApp::setup() {
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	ofSetLogLevel(OF_LOG_VERBOSE);
	
	string path = "scan/";
	//int proWidth = 1024, proHeight = 768;
	//int proWidth = 3 * 512, proHeight = 256;
	int proWidth = 1024*3, proHeight = 768;
	
	dirHorizontalNormal.listDir(path + "horizontal/normal/");
	dirHorizontalInverse.listDir(path + "horizontal/inverse/");
	dirVerticalNormal.listDir(path + "vertical/normal/");
	dirVerticalInverse.listDir(path + "vertical/inverse/");
	hnFiles = dirHorizontalNormal.getFiles();
	hiFiles = dirHorizontalInverse.getFiles();
	vnFiles = dirVerticalNormal.getFiles();
	viFiles = dirVerticalInverse.getFiles();
	ofSort(hnFiles, natural);
	ofSort(hiFiles, natural);
	ofSort(vnFiles, natural);
	ofSort(viFiles, natural);
	horizontalBits = dirHorizontalNormal.size();
	verticalBits = dirVerticalNormal.size();
	
	cameraMask.loadImage(path + "cameraMask.png");
	cameraMask.setImageType(OF_IMAGE_GRAYSCALE);
	
	ofImage prototype;
	prototype.loadImage(path + "horizontal/normal/0.jpg");
	camWidth = prototype.getWidth(), camHeight = prototype.getHeight();
	
	camConfidence = Mat::zeros(camHeight, camWidth, CV_32FC1);
	binaryCodedHorizontal = Mat::zeros(camHeight, camWidth, CV_16UC1);
	binaryCodedVertical = Mat::zeros(camHeight, camWidth, CV_16UC1);
	
	Mat cameraMaskMat = toCv(cameraMask);
	
	for(int i = 0; i < horizontalBits; i++) {
		processGraycodeLevel(i, horizontalBits, 2, hnFiles[i].path(), hiFiles[i].path(), cameraMaskMat, camConfidence, binaryCodedHorizontal);
	}
	for(int i = 0; i < verticalBits; i++) {
		processGraycodeLevel(i, verticalBits, 2, vnFiles[i].path(), viFiles[i].path(), cameraMaskMat, camConfidence, binaryCodedVertical);
	}
	grayToBinary(binaryCodedHorizontal, horizontalBits);
	grayToBinary(binaryCodedVertical, verticalBits);
	
	ofLogVerbose() << "building proMap";
	proMap = Mat::zeros(proHeight, proWidth, CV_16UC3);
	proConfidence = Mat::zeros(proHeight, proWidth, CV_32FC1);
	
	for(int cy = 0; cy < camHeight; cy++) {
		for(int cx = 0; cx < camWidth; cx++) {
			float curCamConfidence = camConfidence.at<float>(cy, cx);
			unsigned short px = binaryCodedVertical.at<unsigned short>(cy, cx);
			unsigned short py = binaryCodedHorizontal.at<unsigned short>(cy, cx);
			if(px < proWidth && py < proHeight) {
				float curProConfidence = proConfidence.at<float>(py, px);
				if(curCamConfidence > curProConfidence) {
					proConfidence.at<float>(py, px) = curCamConfidence;
					Vec3w& curProMap = proMap.at<Vec3w>(py, px);
					curProMap[0] = cx;
					curProMap[1] = cy;
				}
			}
		}
	}
	
	ofLogVerbose() << "saving proConfidence";
	ofFloatPixels proConfidencePixels;
	toOf(proConfidence, proConfidencePixels);
	ofSaveImage(proConfidencePixels, "proConfidence.exr");
	
	ofLogVerbose() << "saving proMap";
	ofShortPixels proMapPixels;
	toOf(proMap, proMapPixels);
	ofSaveImage(proMapPixels, "proMap.png");
	
	ofLogVerbose() << "saving camConfidence";
	ofFloatPixels camConfidencePixels;
	toOf(camConfidence, camConfidencePixels);
	ofSaveImage(camConfidencePixels, "camConfidence.exr");
	
	ofLogVerbose() << "merging binaryCoded";
	ofShortPixels binaryCodedPixels;
	Mat binaryCoded, emptyChannel;
	emptyChannel = Mat::zeros(camHeight, camWidth, CV_16UC1);
	vector<Mat> channels;
	channels.push_back(binaryCodedVertical);
	channels.push_back(binaryCodedHorizontal);
	channels.push_back(emptyChannel);
	merge(channels, binaryCoded);
	toOf(binaryCoded, binaryCodedPixels);
	ofLogVerbose() << "saving binaryCoded";
	ofSaveImage(binaryCodedPixels, "binaryCoded.png");
}

void testApp::update() {
}

void testApp::draw() {
}
