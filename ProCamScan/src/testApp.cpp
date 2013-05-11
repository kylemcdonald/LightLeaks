#include "testApp.h"

#include "LightLeaksUtilities.h"

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

void processGraycodeLevel(int i, int n, int dimensions, Mat cameraMask, Mat& confidence, Mat& binaryCoded, Mat& minMat, Mat& maxMat, ofImage * imageNormal, ofImage * imageInverse) {
    ofLogVerbose() << "Process " << i << " of " << n;

	int w = imageNormal->getWidth(), h = imageNormal->getHeight();
	cv::Mat imageNormalGray, imageInverseGray;
	convertColor(*imageNormal, imageNormalGray, CV_RGB2GRAY);
	convertColor(*imageInverse, imageInverseGray, CV_RGB2GRAY);
    
    if(cameraMask.cols > 0){
        imageNormalGray &= cameraMask;
        imageInverseGray &= cameraMask;
    }
    
	if(i == 0) {
		minMat = min(imageNormalGray, imageInverseGray);
		maxMat = max(imageNormalGray, imageInverseGray);
	} else {
		min(minMat, imageNormalGray, minMat);
		min(minMat, imageInverseGray, minMat);
		max(maxMat, imageNormalGray, maxMat);
		max(maxMat, imageInverseGray, maxMat);
	}
	float totalVariation = 0;
	for(int j = 0; j < n; j++) {
		totalVariation += 1 << (n - j - 1);
	}
	unsigned short curMask = 1 << (n - i - 1);
	float curVariation = curMask / (dimensions * 255. * totalVariation);
    
#pragma omp parallel for
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
	
	string path = "scan/";
	
    cout << "-----------------"<<endl<<" -- ProCamScan --"<<endl<<"-----------------"<<endl;
    cout << "Scanning data/"+path+" for images"<<endl;;
    
	dirHorizontalNormal.listDir(path + "horizontal/normal/");
	dirHorizontalInverse.listDir(path + "horizontal/inverse/");
	dirVerticalNormal.listDir(path + "vertical/normal/");
	dirVerticalInverse.listDir(path + "vertical/inverse/");
	hnFiles = dirHorizontalNormal.getFiles();
	hiFiles = dirHorizontalInverse.getFiles();
	vnFiles = dirVerticalNormal.getFiles();
	viFiles = dirVerticalInverse.getFiles();
    
    ofSetLogLevel(OF_LOG_VERBOSE);

	ofSort(hnFiles, natural);
	ofSort(hiFiles, natural);
	ofSort(vnFiles, natural);
	ofSort(viFiles, natural);
	horizontalBits = dirHorizontalNormal.size();
	verticalBits = dirVerticalNormal.size();
    
    //
    //Error handling
    //
    if(horizontalBits == 0){
        ofLogError() << "No horizontal images found (searching in data/"+path+"horizontal/normal). Quitting";
        ofExit();
    }
    if(verticalBits == 0){
        ofLogError() << "No vertical images found (searching in data/"+path+"vertical/normal). Quitting";
        ofExit();
    }
    if(dirHorizontalInverse.size() != dirHorizontalNormal.size()){
        ofLogError() << "Mismatch in number of horizontal images ("+ofToString(dirHorizontalNormal.size())+" normal images and "+ofToString(dirHorizontalInverse.size())+" inverse images)";
        ofExit();
    }
    if(dirVerticalInverse.size() != dirVerticalNormal.size()){
        ofLogError() << "Mismatch in number of vertical images ("+ofToString(dirVerticalNormal.size())+" normal images and "+ofToString(dirVerticalInverse.size())+" inverse images)";
        ofExit();
    }
    
	//
    //Camera mask
    //
	bool maskLoaded = cameraMask.loadImage(path + "cameraMask.png");
	cameraMask.setImageType(OF_IMAGE_GRAYSCALE);
    if(!maskLoaded){
        ofLogVerbose() << "No file called cameraMask.png in data/scan folder. Continuing without a camera mask";
    } else {
        ofLogVerbose() << "Camera mask loaded";
    }
	
	ofImage prototype;
	prototype.loadImage(path + "horizontal/normal/0.jpg");
	camWidth = prototype.getWidth(), camHeight = prototype.getHeight();
	
	camConfidence = Mat::zeros(camHeight, camWidth, CV_32FC1);
	binaryCodedHorizontal = Mat::zeros(camHeight, camWidth, CV_16UC1);
	binaryCodedVertical = Mat::zeros(camHeight, camWidth, CV_16UC1);
	
	Mat cameraMaskMat;
    if(maskLoaded){
        cameraMaskMat = toCv(cameraMask);
    }
	//imitate(minImage, cameraMask);
	//imitate(maxImage, cameraMask);

    
    hnImageNormal.resize(horizontalBits, 0);
    hnImageInverse.resize(horizontalBits, 0);

    viImageNormal.resize(verticalBits, 0);
    viImageInverse.resize(verticalBits, 0);

    
    //
    //Load images
    //
    
    ofLogVerbose() << "Loading " <<  horizontalBits << " horizontal images multithreaded";

#pragma omp parallel for
	for(int i = 0; i < horizontalBits; i++) {
        ofImage * img = new ofImage();
        img->setUseTexture(false);

        ofImage * imgI = new ofImage();
        imgI->setUseTexture(false);

        img->loadImage(hnFiles[i].path());
        imgI->loadImage(hiFiles[i].path());
        
        hnImageNormal[i] = img;
        hnImageInverse[i] = imgI;
    }
    

    ofLogVerbose() << "Loading " <<  verticalBits << " vertical images multithreaded";
    
#pragma omp parallel for
	for(int i = 0; i < verticalBits; i++) {
        ofImage * img = new ofImage();
        img->setUseTexture(false);
        
        ofImage * imgI = new ofImage();
        imgI->setUseTexture(false);

        img->loadImage(vnFiles[i].path());
        imgI->loadImage(viFiles[i].path());

        viImageNormal[i] = img;
        viImageInverse[i] = imgI;
    }
    
    
    //Process images
    ofLogVerbose() << "Process " <<  horizontalBits << " horizontal images";
	for(int i = 0; i < horizontalBits; i++) {
		processGraycodeLevel(i, horizontalBits, 2, cameraMaskMat, camConfidence, binaryCodedHorizontal, minImage, maxImage, hnImageNormal[i], hnImageInverse[i]);
	}

    ofLogVerbose() << "Process " <<  verticalBits << " vertical images";
	for(int i = 0; i < verticalBits; i++) {
		processGraycodeLevel(i, verticalBits, 2, cameraMaskMat, camConfidence, binaryCodedVertical, minImage, maxImage, viImageNormal[i], viImageInverse[i]);
	}
    
	grayToBinary(binaryCodedHorizontal, horizontalBits);
	grayToBinary(binaryCodedVertical, verticalBits);
	
	ofLogVerbose() << "saving results";
	//saveImage(camConfidence, "camConfidence.exr");
	//saveImage(minImage, "minImage.png");
	//saveImage(maxImage, "maxImage.png");
	
	ofLogVerbose() << "saving binaryCoded";
	Mat binaryCoded, emptyChannel;
	emptyChannel = Mat::zeros(camHeight, camWidth, CV_16UC1);
	vector<Mat> channels;
	channels.push_back(binaryCodedVertical);
	channels.push_back(binaryCodedHorizontal);
	channels.push_back(emptyChannel);
	merge(channels, binaryCoded);
	//saveImage(binaryCoded, "binaryCoded.png");
    
    ofLogVerbose() << "Build Pro Map";

    proWidth = 1024 * 3, proHeight = 768;
	buildProMap(proWidth, proHeight,
							binaryCoded,
							camConfidence,
							proConfidence,
							proMap,
							mean,
							stddev,
							count);
	
	saveImage(proConfidence, "proConfidence.exr");
	saveImage(proMap, "proMap.png");
	//saveImage(mean, "mean.png");
	//saveImage(stddev, "stddev.exr");
	//saveImage(count, "count.png");
    
    ofLogVerbose() <<" Done in "+ofToString(ofGetElapsedTimef())+" seconds";
}

void testApp::update() {
}

void testApp::draw() {
}
