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
    imageNormalGray = toCv(*imageNormal);
    imageInverseGray = toCv(*imageInverse);
    
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
	
	
    cout << "-----------------"<<endl<<" -- ProCamScan --"<<endl<<"-----------------"<<endl;
    //    ofSetDataPathRoot(<#string root#>)
    setCalibrationDataPathRoot();
    
    ofSetLogLevel(OF_LOG_VERBOSE);
    
    

    vector<ofFile> scans = getScanNames();
    for(int scan=0;scan<scans.size();scan++){
        string scanName = scans[scan].getFileName();
        string path = scans[scan].path()+"/";

        string camMaskPath = path+"/mask.png";
        ofFile proConfidenceFile = ofFile(path+"/proConfidence.exr");
        ofFile proMapFile = ofFile(path+"/proMap.png");

        bool outputFilesExist = proConfidenceFile.exists() || proMapFile.exists();
        if(outputFilesExist){
            ofLogVerbose()<<"Skipping "<<scanName<<" since output files (SharedData/"<<scanName<<"proMaps) already exist";
        }
        if(scanName[0] == '_'){
            ofLogVerbose()<<"Skipping "<<scanName<<" since it's underscored";
        }
        
        //Skip folders with underscore
        if(!outputFilesExist && scanName[0] != '_'){
            
            ofLogVerbose()<<"ProCamScan "<<scanName<<endl;
            
            
            ofSetLogLevel(OF_LOG_ERROR);
            
            dirHorizontalNormal.listDir(path + "cameraImages/horizontal/normal/");
            dirHorizontalInverse.listDir(path + "cameraImages/horizontal/inverse/");
            dirVerticalNormal.listDir(path + "cameraImages/vertical/normal/");
            dirVerticalInverse.listDir(path + "cameraImages/vertical/inverse/");
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
                ofLogError() << "No horizontal images found (searching in SharedData/"+path+"cameraImages/horizontal/normal). Quitting";
                ofExit();
            }
            if(verticalBits == 0){
                ofLogError() << "No vertical images found (searching in SharedData/"+path+"cameraImages/vertical/normal). Quitting";
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
            bool maskLoaded = cameraMask.loadImage(camMaskPath);
            cameraMask.setImageType(OF_IMAGE_GRAYSCALE);
            if(!maskLoaded){
                ofLogVerbose() << "No file called mask.png in SharedData/"+scanName+" folder. Continuing without a camera mask";
            } else {
                ofLogVerbose() << "Camera mask loaded";
            }
            
            ofImage prototype;
            prototype.loadImage(path + "cameraImages/horizontal/normal/0.jpg");
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
            //Load Horizontal images
            //
            
            ofLogVerbose() << "Loading " <<  horizontalBits << " horizontal images multithreaded";
            
#pragma omp parallel for
            for(int i = 0; i < horizontalBits; i++) {
                ofImage * img = new ofImage();
                img->setUseTexture(false);
                
                ofImage * imgI = new ofImage();
                imgI->setUseTexture(false);
                
                //cout<<"Load "+hnFiles[i].path()<<endl;
                img->loadImage(hnFiles[i].path());
                imgI->loadImage(hiFiles[i].path());

                img->setImageType(OF_IMAGE_GRAYSCALE);
                imgI->setImageType(OF_IMAGE_GRAYSCALE);

                hnImageNormal[i] = img;
                hnImageInverse[i] = imgI;
            }
            
            //Process horizontal images
            ofLogVerbose() << "Process " <<  horizontalBits << " horizontal images";
            for(int i = 0; i < horizontalBits; i++) {
                processGraycodeLevel(i, horizontalBits, 2, cameraMaskMat, camConfidence, binaryCodedHorizontal, minImage, maxImage, hnImageNormal[i], hnImageInverse[i]);
            }            
            
            //Delete horizontal images
            for(int i = 0; i < horizontalBits; i++) {
                delete hnImageNormal[i];
                delete hnImageInverse[i];
            }
            
            
            //
            //Load Vertical images
            //            
            ofLogVerbose() << "Loading " <<  verticalBits << " vertical images multithreaded";
#pragma omp parallel for
            for(int i = 0; i < verticalBits; i++) {
                ofImage * img = new ofImage();
                img->setUseTexture(false);
                
                ofImage * imgI = new ofImage();
                imgI->setUseTexture(false);
                
                img->loadImage(vnFiles[i].path());
                imgI->loadImage(viFiles[i].path());
                
                img->setImageType(OF_IMAGE_GRAYSCALE);
                imgI->setImageType(OF_IMAGE_GRAYSCALE);
                
                viImageNormal[i] = img;
                viImageInverse[i] = imgI;
            }

            
            //Process vertical images
            ofLogVerbose() << "Process " <<  verticalBits << " vertical images";
            for(int i = 0; i < verticalBits; i++) {
                processGraycodeLevel(i, verticalBits, 2, cameraMaskMat, camConfidence, binaryCodedVertical, minImage, maxImage, viImageNormal[i], viImageInverse[i]);
            }

            //Delete vertical images
            for(int i = 0; i < verticalBits; i++) {
                delete viImageNormal[i];
                delete viImageInverse[i];
            }            
            
            hnImageInverse.clear();
            hnImageNormal.clear();
            
            viImageInverse.clear();
            viImageNormal.clear();  

 
            grayToBinary(binaryCodedHorizontal, horizontalBits);
            grayToBinary(binaryCodedVertical, verticalBits);
            
            //ofLogVerbose() << "saving results";
            //saveImage(camConfidence, "camConfidence.exr");
            //saveImage(minImage, "minImage.png");
            //saveImage(maxImage, "maxImage.png");
            
            Mat binaryCoded, emptyChannel;
            emptyChannel = Mat::zeros(camHeight, camWidth, CV_16UC1);
            vector<Mat> channels;
            channels.push_back(binaryCodedVertical);
            channels.push_back(binaryCodedHorizontal);
            channels.push_back(emptyChannel);
            merge(channels, binaryCoded);
            //ofLogVerbose() << "saving binaryCoded";
            //saveImage(binaryCoded, "binaryCoded.png");
            
            ofLogVerbose() << "Build Pro Map";
            
            
            proWidth = 1920, proHeight = 1080;
            buildProMap(proWidth, proHeight,
                        binaryCoded,
                        camConfidence,
                        proConfidence,
                        proMap);

            saveImage(proConfidence, path+"/proConfidence.exr");
            saveImage(proMap, path+"/proMap.png");
             
             
            //saveImage(mean, "mean.png");
            //saveImage(stddev, "stddev.exr");
            //saveImage(count, "count.png");
            
            
            
        }
    }
    ofLogVerbose() <<" Done in "+ofToString(ofGetElapsedTimef())+" seconds";
}

void testApp::update() {
}

void testApp::draw() {
}
