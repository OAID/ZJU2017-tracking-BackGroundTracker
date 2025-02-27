#include <iostream>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
//#include "highgui.h"
#include "colotracker.h"
#include "region.h"
#include <string>
#include <ctime>
#include "background_alg.h"

#define USE_VIDEO_FILE

using namespace cv;
using namespace std;
std::string videoPath;
clock_t clockBegin = 0, clockEnd = 0;
//float resize_factor_color = 1;
float resize_fac = 0.25;
bool isBackReady = false;

cv::Point g_topLeft(0,0);
cv::Point g_botRight(0,0);
cv::Point g_botRight_tmp(0,0);
bool plot = false;
bool g_trackerInitialized = false;
ColorTracker * g_tracker = NULL;


static void onMouse( int event, int x, int y, int, void* param)
{
    cv::Mat img = ((cv::Mat *)param)->clone();
    if( event == cv::EVENT_LBUTTONDOWN && !g_trackerInitialized){
        std::cout << "DOWN " << std::endl;
        g_topLeft = Point(x,y);
        plot = true;
    }else if (event == cv::EVENT_LBUTTONUP && !g_trackerInitialized){
        std::cout << "UP " << std::endl;
        g_botRight = Point(x,y);
        plot = false;
        if (g_tracker != NULL)
            delete g_tracker;
        g_tracker = new ColorTracker();
        g_tracker->init(*(cv::Mat *)param, g_topLeft.x, g_topLeft.y, g_botRight.x, g_botRight.y);
        g_trackerInitialized = true;


    }else if (event == cv::EVENT_MOUSEMOVE && !g_trackerInitialized){
        //plot bbox
        g_botRight_tmp = Point(x,y);
        // if (plot){
        //     cv::rectangle(img, g_topLeft, current, cv::Scalar(0,255,0), 2);
        //     imshow("output", img);
        // }
    }
}


int main(int argc, char **argv) 
{
    BBox * bb = NULL;
    cv::Mat img, img_resize;
#ifndef USE_VIDEO_FILE
    int captureDevice = 0;
    if (argc > 1)
        captureDevice = atoi(argv[1]);
    cv::VideoCapture webcam = cv::VideoCapture(captureDevice);
#else
    if (argc > 1) {
        videoPath = argv[1];
    }
    else{
        std::cout << "Usage: ./tracker_B13 ./path_to_video/1.AVI" << std::endl;
        return 1;
    }
    cv::VideoCapture webcam = cv::VideoCapture(videoPath);
#endif

    webcam.set(CV_CAP_PROP_FRAME_WIDTH, 640);
    webcam.set(CV_CAP_PROP_FRAME_HEIGHT, 480);

    if (!webcam.isOpened()){
        webcam.release();
        std::cerr << "Error during opening capture device!" << std::endl;
        return 1;
    }
    cv::namedWindow( "output", 0 );
    cv::setMouseCallback( "output", onMouse, &img);

#ifdef USE_VIDEO_FILE
    webcam >> img;
    cv::resize(img, img_resize, Size(img.cols*resize_fac, img.rows*resize_fac));
    while(!g_trackerInitialized){
        if (plot && g_botRight_tmp.x > 0){
           cv::rectangle(img, g_topLeft, g_botRight_tmp, cv::Scalar(0,255,0), 2);
        }
        cv::imshow("output", img);
        cv::waitKey(100);
    }
#else
    while(!g_trackerInitialized){
        webcam >> img;
        cv::resize(img, img_resize, Size(img.cols*resize_fac, img.rows*resize_fac));
        if (plot && g_botRight_tmp.x > 0){
               cv::rectangle(img, g_topLeft, g_botRight_tmp, cv::Scalar(0,0,255), 2);
        }
        cv::imshow("output", img);
        cv::waitKey(50);
    }
#endif

    background_alg b_tracker = background_alg(img.cols*resize_fac, img.rows*resize_fac, 1, resize_fac, 0.05);
    b_tracker.background_update(img_resize, Point2i(g_topLeft.x*resize_fac, g_topLeft.y*resize_fac),
                                            Point2i(g_botRight.x*resize_fac, g_botRight.y*resize_fac));
    for(;;){
        Rect colorResultRect;
        Rect backResultRect;

        webcam >> img;
        cv::resize(img, img_resize, Size(img.cols*resize_fac, img.rows*resize_fac));

        if (g_trackerInitialized){
            clockBegin = clock();
            if(!isBackReady){
                bb = g_tracker->track(img);
                colorResultRect = Rect(bb->x, bb->y, bb->width, bb->height);
                b_tracker.background_update(img_resize, Point2i(bb->x*resize_fac, bb->y*resize_fac),
                                                        Point2i((bb->x+bb->width)*resize_fac, (bb->y+bb->height)*resize_fac));
                isBackReady = b_tracker.background_isReady();
                cv::rectangle(img, colorResultRect, Scalar(255, 0, 0), 3);
            }else {
                backResultRect = b_tracker.object_detect(img_resize);
                b_tracker.background_update(img_resize,
                                            Point2i(backResultRect.x, backResultRect.y),
                                            Point2i((backResultRect.x + backResultRect.width),
                                                    (backResultRect.y + backResultRect.height)));
                cv::rectangle(img, Rect(backResultRect.x / resize_fac, backResultRect.y / resize_fac,
                                        backResultRect.width / resize_fac, backResultRect.height / resize_fac),
                                        Scalar(0, 255, 0), 3);
            }
            clockEnd = clock();
            static int clock_cnt = 0;
            static int clock_sum = 0;
            clock_cnt++;
            clock_sum += (clockEnd - clockBegin)/1000;
            if(clock_cnt>=10) {
                std::cout << "=============================Average time： " << clock_sum/clock_cnt << " ms" << std::endl;
                clock_cnt = 0;
                clock_sum = 0;
            }
        }
        cv::imshow("output", img);
#ifdef USE_VIDEO_FILE
        int c = waitKey(100);
#else
        int c = waitKey(10);
#endif
        if((c & 255) == 27) { std::cout << "Exiting ..." << std::endl; break;}
    }
    return 0;
}
