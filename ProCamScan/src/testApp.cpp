#include "testApp.h"

using namespace ofxCv;
using namespace cv;

void processGraycodeLevel(int i, int n, int dimensions, string normalPath, string inversePath, Mat& confidence, Mat& binaryCoded) {
	ofLogVerbose() << "loading " << normalPath <<  "/" << inversePath << " " << i << " of " << n;
	ofImage imageNormal, imageInverse;
	imageNormal.loadImage(normalPath);
	imageInverse.loadImage(inversePath);
	ofLogVerbose() << "processing";
	cv::Mat imageNormalGray, imageInverseGray;
	convertColor(imageNormal, imageNormalGray, CV_RGB2GRAY);
	convertColor(imageInverse, imageInverseGray, CV_RGB2GRAY);
	unsigned char* normalPixels = imageNormalGray.ptr<unsigned char>();
	unsigned char* inversePixels = imageInverseGray.ptr<unsigned char>();
	float* confidencePixels = confidence.ptr<float>();
	unsigned short* binaryCodedPixels = binaryCoded.ptr<unsigned short>();
	int m = imageNormal.getWidth() * imageNormal.getHeight();
	float totalVariation = 0;
	for(int j = 0; j < n; j++) {
		totalVariation += (1 << j);
	}
	float curVariation = (1 << i) / (dimensions * 255. * totalVariation);
	unsigned short curMask = 1 << (n - i - 1);
	for(int j = 0; j < m; j++) {
		unsigned char normal = normalPixels[j], inverse = inversePixels[j];
		if(normal > inverse) {
			binaryCodedPixels[j] |= curMask;
		}
		float range = fabsf((float) normal - (float) inverse);
		confidencePixels[j] += range * curVariation; 
	}
	grayToBinary(binaryCoded, n);
}

void testApp::setup() {
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	ofSetLogLevel(OF_LOG_VERBOSE);
	
	string path = "qs/";
	int proWidth = 3 * 1024, proHeight = 768;
	
	dirHorizontalNormal.listDir(path + "horizontal/normal/");
	dirHorizontalInverse.listDir(path + "horizontal/inverse/");
	dirVerticalNormal.listDir(path + "vertical/normal/");
	dirVerticalInverse.listDir(path + "vertical/inverse/");
	n = dirHorizontalNormal.size();
	
	ofImage prototype;
	prototype.loadImage(path + "horizontal/normal/0.jpg");
	camWidth = prototype.getWidth(), camHeight = prototype.getHeight();
	
	confidence = Mat::zeros(camHeight, camWidth, CV_32FC1);
	binaryCodedHorizontal = Mat::zeros(camHeight, camWidth, CV_16UC1);
	binaryCodedVertical = Mat::zeros(camHeight, camWidth, CV_16UC1);
	
	for(int i = 0; i < n; i++) {
		processGraycodeLevel(i, n, 2, dirHorizontalNormal.getPath(i), dirHorizontalInverse.getPath(i), confidence, binaryCodedHorizontal);
		processGraycodeLevel(i, n, 2, dirVerticalNormal.getPath(i), dirVerticalInverse.getPath(i), confidence, binaryCodedVertical);
	}
	
	ofFloatPixels confidencePixels;
	ofShortPixels binaryCodedPixels;
	Mat binaryCoded, emptyChannel;
	emptyChannel = Mat::zeros(camHeight, camWidth, CV_16UC1);
	vector<Mat> channels;
	channels.push_back(binaryCodedHorizontal);
	channels.push_back(binaryCodedVertical);
	channels.push_back(emptyChannel);
	merge(channels, binaryCoded);
	toOf(confidence, confidencePixels);
	toOf(binaryCoded, binaryCodedPixels);
	ofLogVerbose() << "saving confidence";
	ofSaveImage(confidencePixels, "confidence.exr");
	ofLogVerbose() << "saving binaryCoded";
	ofSaveImage(binaryCodedPixels, "binaryCoded.png");
}

void testApp::update() {
}

void testApp::draw() {
}
