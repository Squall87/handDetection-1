#include <Windows.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <opencv2/highgui/highgui.hpp>
#include "opencv2/imgproc/imgproc.hpp"
#include <ctime>

using namespace cv;
using namespace std;

int erosion_elem = 0;
int erosion_size = 3;
int dilation_elem = 0;
int dilation_size = 4;
int const max_elem = 2;
int const max_kernel_size = 21;
Scalar avgSkinCol[6];
int numSamples = 6;

//create elements for morphological tranforms
Mat eroElement = getStructuringElement(MORPH_RECT,
	Size(2 * erosion_size + 1, 2 * erosion_size + 1),
	Point(erosion_size, erosion_size));

Mat dilElement = getStructuringElement(MORPH_RECT,
	Size(2 * dilation_size + 1, 2 * dilation_size + 1),
	Point(dilation_size, dilation_size));

//function headers
Mat filterHand(VideoCapture cap);
Scalar detectSkinColor(Mat frame, Rect scanBox);
Mat scanBox(Rect box, Mat frame, int boxNum);
void detectHand(Mat frame);
vector<Vec4i> findFingerDefects(vector<Vec4i> defects);

int clamp(int n){
	int x = n > 255? 255 : n;
			x < 0?	 0 : x;
	
	return x;
}

int main(){
	Mat frame;

	// open the default camera
	VideoCapture cap(0);

	// check if we succeeded
	if (!cap.isOpened())
		return -1;

	frame = filterHand(cap);

	detectHand(frame);
	//HSVMethod();

	waitKey(0);

	return 0;
}

void detectHand(Mat frame){

}

Mat filterHand(VideoCapture cap){
	//performance measurements
	clock_t begin, end;
	double tGaussBlr, tThreshold, tCombine, tMedBlur, tMorph, tDisplayResults;
	Point mousePos(0,0);

	Mat finFrame;

	while (1){
		Mat frame, finImage, showFrame, channel[3], threshImage[6];

		//get frame from camera
		cap >> frame;
		flip(frame, frame, 1);

		//make a copy
		frame.copyTo(showFrame);

		//soften image
		begin = clock();
		//GaussianBlur(frame, frame, Size(5, 5), 0);
		tGaussBlr = double(clock() - begin) / CLOCKS_PER_SEC;

		cvtColor(frame, frame, CV_BGR2HSV);

		//Create ROIs
		int square = 25;
		Rect box1(frame.cols / 2 - square / 2, frame.rows / 2 - (5 * square / 2), square, square); //up
		Rect box2(frame.cols / 2 - square / 2, frame.rows / 2 - square / 2, square, square); //mid
		Rect box3(frame.cols / 2 - 2 * square, frame.rows / 2 - (square / 2), square, square); //left mid
		Rect box4(frame.cols / 2 - 2 * square, frame.rows / 2 + (3 * square / 2), square, square); //left low
		Rect box5(frame.cols / 2 + square, frame.rows / 2 - (square / 2), square, square); //right mid
		Rect box6(frame.cols / 2 + square, frame.rows / 2 + (3 * square / 2), square, square); //right low

		//threshold the images
		begin = clock();
		threshImage[0] = scanBox(box1, frame, 0);
		threshImage[1] = scanBox(box2, frame, 1);
		threshImage[2] = scanBox(box3, frame, 2);
		threshImage[3] = scanBox(box4, frame, 3);
		threshImage[4] = scanBox(box5, frame, 4);
		threshImage[5] = scanBox(box6, frame, 5);
		tThreshold = double(clock() - begin) / CLOCKS_PER_SEC;

		//combine
		begin = clock();
		finImage = threshImage[0] | threshImage[1] | threshImage[2] | threshImage[3] | threshImage[4] | threshImage[5];
		tCombine = double(clock() - begin) / CLOCKS_PER_SEC;

		//get rid of excess noise
		begin = clock();
		medianBlur(finImage, finImage, 3);
		tMedBlur = double(clock() - begin) / CLOCKS_PER_SEC;

		//opening morphological transform
		begin = clock();
		erode(finImage, finImage, eroElement);
		dilate(finImage, finImage, dilElement);
		tMorph = double(clock() - begin) / CLOCKS_PER_SEC;

		//draw rectangles
		begin = clock();
		rectangle(showFrame, box1, Scalar(0, 0, 255), 2, 8, 0);
		rectangle(showFrame, box2, Scalar(0, 0, 255), 2, 8, 0);
		rectangle(showFrame, box3, Scalar(0, 0, 255), 2, 8, 0);
		rectangle(showFrame, box4, Scalar(0, 0, 255), 2, 8, 0);
		rectangle(showFrame, box5, Scalar(0, 0, 255), 2, 8, 0);
		rectangle(showFrame, box6, Scalar(0, 0, 255), 2, 8, 0);

		//display frame
		namedWindow("frame", CV_WINDOW_AUTOSIZE);
		imshow("frame", showFrame);

		//show results
		namedWindow("finImage", CV_WINDOW_AUTOSIZE);
		imshow("finImage", finImage);
		tDisplayResults = double(clock() - begin) / CLOCKS_PER_SEC;

		if (waitKey(30) >= 0){
			finFrame = finImage;
			break;
		}

	}

	cout << "TIMING: " << endl;
	cout << "Gauss: " << tGaussBlr << endl;
	cout << "tThreshold: " << tThreshold << endl;
	cout << "tCombine: " << tCombine << endl;
	cout << "tMedBlur: " << tMedBlur << endl;
	cout << "tMorph: " << tMorph << endl;
	cout << "tDisplayResults: " << tDisplayResults << endl;

	while (1){
		Mat frame, finImage, showFrame, threshImage[6];
		Scalar lowerCol, upperCol;

		//get frame from camera
		cap >> frame;
		flip(frame, frame, 1);

		//soften image
		GaussianBlur(frame, frame, Size(5, 5), 0);

		cvtColor(frame, frame, CV_BGR2HSV);

		//threshold the images
		for (int i = 0; i<numSamples; i++){
			Scalar lowerSkinCol, upperSkinCol;
			//check overflows - unnecessary?
			lowerSkinCol[0] = clamp(avgSkinCol[i][0] - 30);
			lowerSkinCol[1] = clamp(avgSkinCol[i][1] - 30);
			lowerSkinCol[2] = clamp(avgSkinCol[i][2] - 40);
			upperSkinCol[0] = clamp(avgSkinCol[i][0] + 30);
			upperSkinCol[1] = clamp(avgSkinCol[i][1] + 30);
			upperSkinCol[2] = clamp(avgSkinCol[i][2] + 40);

			inRange(frame,
				Scalar(lowerSkinCol[0], lowerSkinCol[1], lowerSkinCol[2]), //0,55,90
				Scalar(upperSkinCol[0], upperSkinCol[1], upperSkinCol[2]), //28,175,230
				threshImage[i]
				);

			//medianBlur(threshImage[i], threshImage[i], 5);
		}

		//combine
		finImage = threshImage[0] | threshImage[1] | threshImage[2] | threshImage[3] | threshImage[4] | threshImage[5];

		//get rid of excess noise
		medianBlur(finImage, finImage, (1 * 2) + 1);

		

		//opening morphological transform
		erode(finImage, finImage, eroElement);
		dilate(finImage, finImage, dilElement);

		//show results
		namedWindow("finImage", CV_WINDOW_AUTOSIZE);
		imshow("finImage", finImage);

		///save result
		finFrame = finImage;

		/** Find the biggest contour **/
		Mat canny_output;
		vector<vector<Point> > contours;
		vector<Vec4i> hierarchy;
		double area, maxArea = 0;
		int maxContour, numFingers;

		/// Detect edges using canny
		//Canny(finFrame, canny_output, 80, 80 * 2, 3);
		/// Find contours
		findContours(finFrame, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));

		//find the maxmimum area contour
		for (int i = 0; i < contours.size(); i++){
			area = contourArea(contours[i]);
			if (area > maxArea){
				maxArea = area;
				maxContour = i;
			}
		}

		
		if (contours.size() >= 1){

			//find the convex hull
			vector<vector<Point>> hull(contours.size());
			vector<int> hullI;
			convexHull(Mat(contours[maxContour]), hull[maxContour], false); //hull to draw
			convexHull(Mat(contours[maxContour]), hullI, false); //hull to calculate defects

			//find centroid of hand
			Point centroid(0,0);
			for (int i = 0; i < hull[maxContour].size(); i++){
				centroid.x += hull[maxContour][i].x;
				centroid.y += hull[maxContour][i].y;
			}
			centroid.x /= hull[maxContour].size();
			centroid.y /= hull[maxContour].size();

			//find the fingers
			vector<Vec4i> defects, fingerDefects;
			convexityDefects(contours[maxContour], hullI, defects); //find defects
			fingerDefects = findFingerDefects(defects); //eliminate irrelevant defects

			if (fingerDefects.size() >= 1)
				numFingers = fingerDefects.size() - 1;
			else
				numFingers = 0;

			/// Draw contours
			Mat drawing = Mat::zeros(finFrame.size(), CV_8UC3);
			drawContours(drawing, contours, maxContour, Scalar(0, 0, 255), 2, 8, hierarchy, 0, Point());
			drawContours(drawing, hull, maxContour, Scalar(0, 255, 0), 2, 8, hierarchy, 0, Point());
			///Draw Centroid
			circle(drawing, centroid, 5, Scalar(255, 0, 0), 10, 8, 0);
			
			//move cursor
			int accel = 2;
			Point newMousePos(centroid.x * accel, centroid.y * accel);
			SetCursorPos(newMousePos.x, newMousePos.y);
			mousePos = newMousePos;

			/// Show in a window
			string fingerCount = "fingers: " + to_string(numFingers);
			putText(drawing, fingerCount, Point(20, 20), FONT_HERSHEY_PLAIN, 2, Scalar(255, 255, 0), 4, 8, 0);
			namedWindow("Contours", CV_WINDOW_AUTOSIZE);
			imshow("Contours", drawing);

		}
		if (waitKey(30) >= 0){
			break;
		}
	}



	return finFrame;
}

Mat scanBox(Rect box, Mat frame, int boxNum){
	Mat threshImage;

	avgSkinCol[boxNum] = detectSkinColor(frame, box);

	//GaussianBlur(frame, frame, Size(5,5), 0);
	Scalar lowerSkinCol, upperSkinCol;
	//check overflows - unnecessary?
	lowerSkinCol[0] = clamp(avgSkinCol[boxNum][0] - 30);
	lowerSkinCol[1] = clamp(avgSkinCol[boxNum][1] - 30);
	lowerSkinCol[2] = clamp(avgSkinCol[boxNum][2] - 40);
	upperSkinCol[0] = clamp(avgSkinCol[boxNum][0] + 30);
	upperSkinCol[1] = clamp(avgSkinCol[boxNum][1] + 30);
	upperSkinCol[2] = clamp(avgSkinCol[boxNum][2] + 40);

	//threshold skin
	inRange(frame,
		Scalar(lowerSkinCol[0], lowerSkinCol[1], lowerSkinCol[2]), //0,55,90
		Scalar(upperSkinCol[0], upperSkinCol[1], upperSkinCol[2]), //28,175,230
		threshImage[boxNum]
		);

	//medianBlur(threshImage, threshImage, 5);

	return threshImage;
}

Scalar detectSkinColor(Mat frame, Rect scanBox){

	//create Mat with ROI
	Mat skinFrame(frame, scanBox);
	//GaussianBlur(skinFrame, skinFrame, Size(15,15), 0);

	//find the average value
	Scalar avgSkinColor = mean(skinFrame); //perhaps get median?

	//cout << "avg Color" << avgSkinColor << endl;

	return avgSkinColor;
}

vector<Vec4i> findFingerDefects(vector<Vec4i> defects){
	vector<Vec4i> fingerDefects;
	double minDepth = 2500;

	for (int i = 0; i < defects.size(); i++){
		//cout << "depth is: " << defects[i][3] << endl;
		if (defects[i][3] > minDepth){
			fingerDefects.push_back(defects[i]);
		}
	}

	return fingerDefects;
}
