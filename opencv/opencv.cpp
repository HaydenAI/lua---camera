//==============================================================================
// File: opencv
//
// Description: A wrapper for opencv camera frame grabber
//
// Created: November 8, 2011, 4:08pm
//
// Author: Jordan Bates // jtbates@gmail.com
//         Using code from Clement Farabet's wrappers for camiface and v4l
//==============================================================================

#include <luaT.h>
#include <TH.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <opencv2/opencv.hpp>

#include <highgui.h>
#include <thread>
#include <mutex>
#include <atomic>

#define MAXIDX 100
static cv::VideoCapture cap;
static int fidx = 0;
cv::Mat frame;

std::thread cap_thread;
std::mutex cap_mutex;
std::atomic_bool done;

extern "C" int l_initCam(lua_State *L) {

    done = false;

    // args
    //int width = lua_tonumber(L, 2);
    //int height = lua_tonumber(L, 3);

    // max allocs ?
    if (fidx == MAXIDX) {
        perror("max nb of devices reached...\n");
    }

    // if number, open a camera device
    if (lua_isnumber(L, 1) || lua_isstring(L, 4)) {
        printf("initializing camera\n");
        const int idx = lua_tonumber(L, 1);
        if (idx < 0) {
            const std::string stream = lua_tostring(L, 4);
            if(stream.empty()) {
                perror("Stream string not set");
            }
            cap.open(stream, cv::CAP_GSTREAMER);
        } else {
            cap.open(idx);
        }

        if (!cap.isOpened()) {
            perror("could not create OpenCV capture");
        }
        //    sleep(2);
        //cvSetCaptureProperty(capture[fidx], CV_CAP_PROP_FRAME_WIDTH, width);
        //cvSetCaptureProperty(capture[fidx], CV_CAP_PROP_FRAME_HEIGHT, height);
        cap.read(frame);
        int tries = 10;
        //while ((!frame[fidx] || frame[fidx]->height != height || frame[fidx]->width != width) && tries>0) {
        //// The above while should only be used with cvSetCaptureProperty

        while (frame.empty() && tries > 0) {
            cap.read(frame);
            tries--;
            sleep(1);
        }
        if (frame.empty()) {
            perror("failed OpenCV test capture");
        }

        printf("camera initialized\n");
    }

    // next
    lua_pushnumber(L, fidx);
    fidx++;

    cap_thread = std::thread([&](){
    	while (!done){	
		// grab frame
    		cv::Mat img;
		    cap.read(img);
		    if (img.empty()) {
		        perror("could not query OpenCV capture");
		    }

            std::unique_lock<std::mutex> guard(cap_mutex);
            frame = img.clone();
		    guard.unlock();
		    cv::waitKey(50);
    	}

    });

    return 1;
}

// frame grabber
extern "C"  int l_grabFrame(lua_State *L) {
    // Get Tensor's Info
    const int idx = lua_tonumber(L, 1);
    THFloatTensor *tensor = (THFloatTensor *) luaT_toudata(L, 2, "torch.FloatTensor");


    std::unique_lock<std::mutex> guard(cap_mutex);
    cv::Mat local_frame = frame.clone();
    guard.unlock();
    // resize given tensor
    THFloatTensor_resize3d(tensor, 3, local_frame.rows, local_frame.cols);

    // copy to tensor
    int m0 = tensor->stride[1];
    int m1 = tensor->stride[2];
    int m2 = tensor->stride[0];

    int channels = local_frame.channels();
    unsigned char *src = (unsigned char *) local_frame.data;
    float *dst = THFloatTensor_data(tensor);
    int i, j, k;
    for (i = 0; i < local_frame.rows; i++) {
        for (j = 0, k = 0; j < local_frame.cols; j++, k += m1) {
            // red:
            dst[k] = src[i * local_frame.step + j * channels + 2] / 255.;
            // green:
            dst[k + m2] = src[i * local_frame.step + j * channels + 1] / 255.;
            // blue:
            dst[k + 2 * m2] = src[i * local_frame.step + j * channels + 0] / 255.;
        }
        dst += m0;
    }
    return 0;
}

extern "C"  int l_releaseCam(lua_State *L) {

    done = true;
    if(cap_thread.joinable()){
        cap_thread.join();
    }
    cap.release();
    return 0;
}

// Register functions
const struct luaL_reg opencv[] = {
        {"initCam",    l_initCam},
        {"grabFrame",  l_grabFrame},
        {"releaseCam", l_releaseCam},
        {NULL, NULL}  /* sentinel */
};

extern "C" int luaopen_libcamopencv(lua_State *L) {
    luaL_openlib(L, "libcamopencv", opencv, 0);
    return 1;
}
