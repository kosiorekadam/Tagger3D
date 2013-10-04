/*
 * Tagger3D.cpp
 *
 *   Created on: 23 sie 2013
 *       Author: Adam Kosiorek
 *	Description:
 */

#include "Tagger3D.h"
#include "../ImgReader/RangeImgReader.h"
#include "../ImgReader/PcdReader.h"
#include "../PointNormal/NormalEstimator.h"
#include "../Detector/SIFTDetector.h"
#include "../Detector/Iss3dDetector.h"
#include "../Descriptor/PFHDescriptor.h"
#include "../Descriptor/FPFHDescriptor.h"
#include "../Cluster/KMeansCluster.h"
#include "../Predictor/SldaPredictor.h"
#include "../Predictor/SVMPredictor.h"

#include <assert.h>
#include <fstream>

namespace Tagger3D {

Tagger3D::Tagger3D(const std::map<std::string, std::string> &configMap) : ProcessObject(configMap) {

	logger = lgr::Logger::getLogger(loggerName);
	DEBUG(logger, "Creating Tagger3D");

	switch( getParam<int>( readerType )) {
	case RANGEIMG_READER: imgReader = std::unique_ptr<ImgReader>(new RangeImgReader(configMap)); break;
	case PCD_READER: imgReader = std::unique_ptr<ImgReader>(new PcdReader(configMap)); break;
	default:
		std::runtime_error e("Invalid reader type");
		ERROR(logger, e.what());
		throw e;
	}

	pointNormal = std::unique_ptr<PointNormal> (new NormalEstimator(configMap));

	switch( getParam<int>( detectorType )) {
	case SIFT: detector = std::unique_ptr<Detector> (new SIFTDetector(configMap)); break;
	case ISS3D: detector = std::unique_ptr<Detector> (new Iss3dDetector(configMap)); break;
	default:
			std::runtime_error e("Invalid detector type");
			ERROR(logger, e.what());
			throw e;
		}

	switch( getParam<int>( descType )) {
	case PFH_DESC: descriptor = std::unique_ptr<Descriptor> (new PFHDescriptor(configMap)); break;
	case FPFH_DESC: descriptor = std::unique_ptr<Descriptor> (new FPFHDescriptor(configMap)); break;
	default:
		std::runtime_error e("Invalid descriptor type");
		ERROR(logger, e.what());
		throw e;
	}

	cluster = std::unique_ptr<Cluster> (new KMeansCluster(configMap));

	switch( getParam<int>( predictorType )) {
	case SLDA: predictor = std::unique_ptr<Predictor> (new SldaPredictor(configMap)); break;
	case SVM: predictor = std::unique_ptr<Predictor> (new SVMPredictor(configMap)); break;
	default:
			std::runtime_error e("Invalid predictor type");
			ERROR(logger, e.what());
			throw e;
	}

	assert( imgReader != nullptr );
	assert( pointNormal != nullptr );
	assert( detector != nullptr );
	assert( descriptor != nullptr );
	assert( cluster != nullptr );
	assert( predictor != nullptr );

	trainDescPath = directory + "/" + trainDescriptors;
	testDescPath = directory + "/" + testDescriptors;

}

Tagger3D::~Tagger3D() {
	DEBUG(logger, "Destroying Tagger3D");
}

int Tagger3D::trainTest() {

	train();
	test();
	return 1;
}
void Tagger3D::train() {
	INFO(logger, "Train");

	std::vector<int> labels = imgReader->readLabels();
	std::vector<cv::Mat> descriptors = prepareDescriptors(trainDescPath);
	std::vector<std::vector<int>> wordDescriptors;

	if(descriptors.size() != labels.size()) {
		std::runtime_error e("Number of point clouds and labels differ: " +
				std::to_string(descriptors.size()) + ", " + std::to_string(labels.size()));
		ERROR(logger, "train: " << e.what());
		throw e;
	}

	prepareCluster(descriptors);


	INFO(logger, "Generating word descriptions");
	wordDescriptors = cluster->cluster(descriptors);

	INFO(logger, "Training classificator");
	predictor->train(wordDescriptors, labels);
}

void Tagger3D::test() {
	INFO(logger, "Test");

	std::vector<int> labels = imgReader->readLabels();
	std::vector<cv::Mat> descriptors = prepareDescriptors(testDescPath);
	std::vector<std::vector<int>> wordDescriptors;

	if(descriptors.size() != labels.size()) {
		std::runtime_error e("Number of point clouds and labels differ: " +
				std::to_string(descriptors.size()) + ", " + std::to_string(labels.size()));
		ERROR(logger, "train: " << e.what());
		throw e;
	}

	cluster->load();

	INFO(logger, "Generating word descriptions");
	wordDescriptors = cluster->cluster(descriptors);

	//predictor->load();
	INFO(logger, "Classifying");
	predictor->predict(wordDescriptors, labels);
}

int Tagger3D::predict(const std::string& rgbPath,
		const std::string& depthPath) {

	ColorCloud::Ptr colorCloud;// = imgReader->readImg(rgbPath, depthPath);
	NormalCloud::Ptr normalCloud = pointNormal->computeNormals( colorCloud );
	ScaleCloud::Ptr keypointCloud = detector->detect( colorCloud );
	cv::Mat descriptors = descriptor->describe( colorCloud, keypointCloud, normalCloud);
	cluster->load();
	std::vector<int> wordDescriptors = cluster->cluster(descriptors);
	return predictor->predict(wordDescriptors)[0];

}

int Tagger3D::run() {

	int m = getParam<int>( mode );
	switch(m) {

	case TRAIN: train(); break;
	case TEST: test(); break;
	case TRAINTEST: trainTest(); break;
	case PREDICT: ; break;
	default:
		std::runtime_error e("Unrecognized mode = " + mode);
		ERROR(logger, "run: " << e.what());
		throw e;
	}
}

const std::string info = ".info";

void Tagger3D::saveDescriptors(const std::vector<cv::Mat>& descriptors,
		const std::string& path) {

	std::ofstream file(path + info, std::ios::binary);
	if(!file.is_open()) {
		std::runtime_error e("Couldn't open the file: " + path + info);
		ERROR(logger, "saveDescriptors: " << e.what());
		throw e;
	}

	size_t images = descriptors.size();
	size_t dims = descriptors[0].cols;
	std::vector<size_t> sizes;
	sizes.reserve(images);
	for(const auto &descriptor : descriptors)
		sizes.push_back(descriptor.rows);

	file.write((char*)&images, sizeof(size_t));
	file.write((char*)&dims, sizeof(size_t));
	file.write((char*)&sizes[0], images * sizeof(size_t));
	file.close();

	file.open(path.c_str(), std::ios::binary);
	if(!file.is_open()) {
		std::runtime_error e("Couldn't open the file: " + path);
		ERROR(logger, "saveDescriptors: " << e.what());
		throw e;
	}

	for(const auto &image : descriptors) {
		for(int i = 0; i < image.rows; i++) {

			char* ptr = (char*)image.ptr<float>(i);
			file.write(ptr, dims * sizeof(float));
		}
	}
	file.close();

}

std::vector<cv::Mat> Tagger3D::loadDescriptors(const std::string& path) {

	std::ifstream file(path + info, std::ios::binary);
	if(!file.is_open()) {
		std::runtime_error e("Couldn't open the file: " + path + info);
		ERROR(logger, "saveDescriptors: " << e.what());
		throw e;
	}

	size_t images;
	size_t dims;


	file.read((char*)&images, sizeof(size_t));
	file.read((char*)&dims, sizeof(size_t));

	std::vector<size_t> sizes;
	sizes.resize(images);
	file.read((char*)&sizes[0], images * sizeof(size_t));

	file.close();


	file.open(path.c_str(), std::ios::binary);
	if(!file.is_open()) {
		std::runtime_error e("Couldn't open the file: " + path);
		ERROR(logger, "saveDescriptors: " << e.what());
		throw e;
	}

	std::vector<cv::Mat> descriptors;
	descriptors.reserve(images);
	for(const auto &size : sizes) {


		//data.resize(size);
		cv::Mat mat(size, dims, CV_32FC1);
		file.read((char*)mat.data, size * dims * sizeof(float));

		//std::cout << mat << std::endl;
		//std::cout << mat.cols << std::endl;
		descriptors.push_back(mat.clone());
	}

	return descriptors;
}

std::vector<cv::Mat> Tagger3D::prepareDescriptors(const std::string &path) {

	std::vector<cv::Mat> descriptors;

	if(getParam<bool>(loadDescriptorsFlag))
		descriptors = loadDescriptors(path);
	else {
		descriptors = computeDescriptors();
		if(getParam<bool>(saveDescriptorsFlag))
			saveDescriptors(descriptors, path);
	}

	return descriptors;
}

std::vector<cv::Mat> Tagger3D::computeDescriptors() {

	INFO(logger, "computeDescriptors");

	std::vector<cv::Mat> descriptors;

	ColorCloud::Ptr colorCloud;
	NormalCloud::Ptr normalCloud;
	ScaleCloud::Ptr keypointCloud;
	cv::Mat descMat;
	int counter = 0;

	colorCloud = imgReader->readImg();
	while(!colorCloud->empty()) {

		normalCloud = pointNormal->computeNormals(colorCloud);
		pointNormal->cleanupInputCloud(colorCloud);
		keypointCloud = detector->detect(colorCloud);
		descMat = descriptor->describe(colorCloud, keypointCloud, normalCloud);
		descriptors.push_back(descMat.clone());
		colorCloud = imgReader->readImg();
		counter++;
	}

	return descriptors;
}

void Tagger3D::prepareCluster(const std::vector<cv::Mat> &descriptors) {

	switch( getParam<int>(trainCluster) ) {
	case 1:
		INFO(logger, "Clusterizing");
		cluster->train(descriptors);
		cluster->save();
		break;
	case 0:
		INFO(logger, "Loading centroids");
		cluster->load();
		break;
	default:
		std::runtime_error e("Invalid cluster operation mode");
		ERROR(logger, e.what());
		throw e;
	}
}



void Tagger3D::computeDescriptorsRun() {
}

void Tagger3D::trainClustRun() {
}

void Tagger3D::trainPredRun() {
}

void Tagger3D::testRun() {
}

} /* namespace Tagger3D */
