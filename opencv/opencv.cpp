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

    cap_thread = std::thread([&]() {
        // grab frame
            while(!done){
                cv::Mat img;
                cap.read(img);
                if (img.empty()) {
                    perror("could not query OpenCV capture");
                }

                std::unique_lock<std::mutex> guard(cap_mutex);
                frame = img.clone();
                guard.unlock();
            }
    	});

    return 1;
}

// frame grabber
extern "C"  int l_grabFrame(lua_State *L) {
    // Get Tensor's Info
    const int idx = lua_tonumber(L, 1);
    THFloatTensor *tensor = (THFloatTensor *) luaT_toudata(L, 2, "torch.FloatTensor");
    float crop_ypos_ratio = lua_tonumber(L, 3);

    int width = lua_tonumber(L, 4);
    int height = lua_tonumber(L, 5);

    std::unique_lock<std::mutex> guard(cap_mutex);
    cv::Mat local_frame = frame.clone();
    guard.unlock();

    float resize_ratio = static_cast<float>(width) / static_cast<float>(local_frame.cols);

    cv::resize(local_frame, local_frame, cv::Size(), resize_ratio, resize_ratio);

    int y  = (local_frame.rows * crop_ypos_ratio) - height;
    if(y < 0){
        y = 0;
    }
    if( y > local_frame.rows - height){
        y = local_frame.rows - height;
    }

    cv::Rect roi(0, y, width, height);
    local_frame = local_frame(roi).clone();


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
            dst[k] = ((src[i * local_frame.step + j * channels + 2] / 255.) -0.3598) / 0.2573;
            // green:
            dst[k + m2] = ((src[i * local_frame.step + j * channels + 1] / 255.) -0.3598) / 0.2663;
            // blue:
            dst[k + 2 * m2] = ((src[i * local_frame.step + j * channels + 0] / 255.) - 0.3598)/ 0.2756;
        }
        dst += m0;
    }
    return 0;
}

extern "C"  int l_convert(lua_State *L) {
    float min = lua_tonumber(L, 1);
    float max = lua_tonumber(L, 2);

    THDoubleTensor *tensor = (THDoubleTensor *) luaT_toudata(L, 3, "torch.DoubleTensor");
    int width = lua_tonumber(L, 4);
    int height = lua_tonumber(L, 5);

    int m0 = tensor->stride[1];
    int m1 = tensor->stride[2];
    int m2 = tensor->stride[0];

    double *src = THDoubleTensor_data(tensor);

    int i, j, k;
    for (i = 0; i < height; i++) {
        for (j = 0, k = 0; j < width; j++, k += m1) {

            // red:
            src[k] = (src[k]- min) /  max;
            // green:
            src[k+m2] = (src[k + m2]- min) /  max;
            // blue:
            src[k + 2 * m2] = (src[k + 2 * m2] - min) /  max;

        }
        src += m0;
    }
    return 0;
}


extern "C"  int l_imageMult(lua_State *L) {
    float scale = lua_tonumber(L, 1);
    THDoubleTensor *tensor = (THDoubleTensor *) luaT_toudata(L, 2, "torch.DoubleTensor");

    int width = lua_tonumber(L, 3);
    int height = lua_tonumber(L, 4);

    int m0 = tensor->stride[1];
    int m1 = tensor->stride[2];
    int m2 = tensor->stride[0];

    double *src = THDoubleTensor_data(tensor);

    int i, j, k;
    for (i = 0; i < height; i++) {
        for (j = 0, k = 0; j < width; j++, k += m1) {

            // red:
            src[k] = src[k] * scale;
            // green:
            src[k+m2] = src[k + m2]* scale;
            // blue:
            src[k + 2 * m2] = src[k + 2 * m2] * scale;

        }
        src += m0;
    }
    return 0;
}

extern "C"  int l_extractLines(lua_State *L) {

    float thresh = lua_tonumber(L, 1);
    float min_points = lua_tonumber(L, 2);
    float line_length = lua_tonumber(L, 3);
    float huber_c = lua_tonumber(L, 4);

    THDoubleTensor *tensor = (THDoubleTensor *) luaT_toudata(L, 5, "torch.DoubleTensor");
    THDoubleTensor *line_tensor = (THDoubleTensor *) luaT_toudata(L, 6, "torch.DoubleTensor");

    int width = lua_tonumber(L, 7);
    int height = lua_tonumber(L, 8);

    int m0 = tensor->stride[1];
    int m1 = tensor->stride[2];
    int m2 = tensor->stride[0];

    double *src = THDoubleTensor_data(tensor);

    cv::Mat dst_mat = cv::Mat::zeros(height, width, CV_8UC3);

    std::vector<std::vector<cv::Point>> line_points(4);

    int i, j, k;
    for (i = 0; i < height; i++) {
        for (j = 0, k = 0; j < width; j++, k += m1) {

            unsigned char * p = dst_mat.ptr(i,j);

            if(src[k] > thresh && j < width/2){
                p[2]  = 255;
                line_points[0].push_back(cv::Point(j,i));
            } else if(src[k + m2] > thresh && j < width/2 && j > width * 0.25){
                p[1]  = 255;
                line_points[1].push_back(cv::Point(j,i));
            } else if(src[k + 2 * m2] > thresh && j > width/2 && j < width * 0.75){
                p[0]  = 255;
                line_points[2].push_back(cv::Point(j,i));
            } else if(src[k + 3 * m2] > thresh && j > width/2){
                p[2]  = 255;
                p[0]  = 255;
                line_points[3].push_back(cv::Point(j,i));
            }
        }
        src += m0;
    }

    std::vector<cv::Vec4f> line_fit(4);
    THDoubleTensor_resize1d(line_tensor, 16);
    double *lt = THDoubleTensor_data(line_tensor);
    int pos = 0;
    for (i = 0; i < line_points.size(); i++) {
        if (line_points[i].size() > min_points) {
            fitLine(line_points[i], line_fit[i], CV_DIST_HUBER, huber_c, 0.01, 0.01);

            lt[pos] = line_fit[i][2] - line_fit[i][0] * line_length;
            lt[pos+1] = line_fit[i][3] - line_fit[i][1] * line_length;
            lt[pos+2] = line_fit[i][2] + line_fit[i][0] * line_length;
            lt[pos+3] = line_fit[i][3] + line_fit[i][1] * line_length;

            cv::line(dst_mat,
                     cv::Point(line_fit[i][2] - line_fit[i][0] * line_length, line_fit[i][3] - line_fit[i][1] * line_length),
                     cv::Point(line_fit[i][2] + line_fit[i][0] * line_length, line_fit[i][3] + line_fit[i][1] * line_length),
                     cv::Scalar(255, 255, 255), 1);
        }
        pos+=4;
    }

    cv::imwrite("lanes.png", dst_mat);

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
        {"convert", l_convert},
        {"imageMult", l_imageMult},
        {"extractLines", l_extractLines},
        {NULL, NULL}  /* sentinel */
};

extern "C" int luaopen_libcamopencv(lua_State *L) {
    luaL_openlib(L, "libcamopencv", opencv, 0);
    return 1;
}
