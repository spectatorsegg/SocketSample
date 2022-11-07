#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include "opencv_test.h"

using namespace cv;

void DisplayImage(const char fname[])
{
    Mat img;

    // Read image file
    img = imread(fname, IMREAD_ANYCOLOR);

    // Create a window
    namedWindow("Display Image", WINDOW_AUTOSIZE);
 
    // Display Image
    imshow("Display Image", img);

    // Wait for key input
    waitKey(0);
    return;
}