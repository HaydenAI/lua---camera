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

#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <pthread.h>

#include <opencv2/opencv.hpp>

#include <highgui.h>

#define MAXIDX 100
static cv::VideoCapture cap;
static cv::Mat frame;
static int fidx = 0;

static int l_initCam(lua_State *L) {
  // args
  //int width = lua_tonumber(L, 2);
  //int height = lua_tonumber(L, 3);

  // max allocs ?
  if (fidx == MAXIDX) {
    perror("max nb of devices reached...\n");
  }

  // if number, open a camera device
  if (lua_isnumber(L, 1)) {
    printf("initializing camera\n");
    const int idx = lua_tonumber(L, 1);
    cap.open(idx);
    if( !cap.isOpened() ) {
      perror("could not create OpenCV capture");
    }
    //    sleep(2);
    //cvSetCaptureProperty(capture[fidx], CV_CAP_PROP_FRAME_WIDTH, width);
    //cvSetCaptureProperty(capture[fidx], CV_CAP_PROP_FRAME_HEIGHT, height);
    cap.read(frame);
    int tries = 10;
    //while ((!frame[fidx] || frame[fidx]->height != height || frame[fidx]->width != width) && tries>0) {
    //// The above while should only be used with cvSetCaptureProperty

    while ( frame.empty() && tries>0) {
      cap.read(frame);
      tries--;
      sleep(1);
    }
    if ( frame.empty()) {
      perror("failed OpenCV test capture");
    }
    
    printf("camera initialized\n");
  }

  // next
  lua_pushnumber(L, fidx);
  fidx ++;
  return 1;
}

// frame grabber
static int l_grabFrame (lua_State *L) {
  // Get Tensor's Info
  const int idx = lua_tonumber(L, 1);
  THFloatTensor * tensor = (THFloatTensor*)luaT_toudata(L, 2, "torch.FloatTensor");

  // grab frame
  cap.read(frame);
  if( frame.empty() ) {
    perror("could not query OpenCV capture");
  }

  // resize given tensor
  THFloatTensor_resize3d(tensor, 3, frame.rows, frame.cols);

  // copy to tensor
  int m0 = tensor->stride[1];
  int m1 = tensor->stride[2];
  int m2 = tensor->stride[0];
  unsigned char *src = (unsigned char *) frame.data;
  float *dst = THFloatTensor_data(tensor);
  int i, j, k;
  for (i=0; i < frame.rows; i++) {
    for (j=0, k=0; j < frame.cols; j++, k+=m1) {
      // red:
      dst[k] = src[i*frame.step + j*frame.channels() + 2]/255.;
      // green:
      dst[k+m2] = src[i*frame.step + j*frame.channels() + 1]/255.;
      // blue:
      dst[k+2*m2] = src[i*frame.step + j*frame.channels() + 0]/255.;
    }
    dst += m0;
  }

  return 0;
}

static int l_releaseCam (lua_State *L) {
  const int idx = lua_tonumber(L, 1);
  cap.release();
  return 0;
}

// Register functions
static const struct luaL_reg opencv [] = {
  {"initCam", l_initCam},
  {"grabFrame", l_grabFrame},
  {"releaseCam", l_releaseCam},
  {NULL, NULL}  /* sentinel */
};

int luaopen_libcamopencv (lua_State *L) {
  luaL_openlib(L, "libcamopencv", opencv, 0);
  return 1;
}
