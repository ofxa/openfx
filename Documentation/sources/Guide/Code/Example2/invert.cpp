/*
Software License :

Copyright (c) 2014, The Open Effects Association Ltd. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name The Open Effects Association Ltd, nor the names of its
      contributors may be used to endorse or promote products derived from this
      software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
  Author : Bruno Nicoletti (2014)

  This plugin will take you through the basics of grabbing and processing
  images with an OFX plugin by grabbing and input image and inverting it.

  The accompanying guide will explain what is happening in more detail.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <iostream>

// the one OFX header we need, it includes the others necessary
#include "ofxImageEffect.h"

#if defined __APPLE__ || defined linux
#  define EXPORT __attribute__((visibility("default")))
#elif defined _WIN32
#  define EXPORT OfxExport
#else
#  error Not building on your operating system quite yet
#endif

////////////////////////////////////////////////////////////////////////////////
// macro to write a labelled message to stderr with
#ifdef _WIN32
  #define DUMP(LABEL, MSG, ...)                                           \
  {                                                                       \
    fprintf(stderr, "%s%s:%d in %s ", LABEL, __FILE__, __LINE__, __FUNCTION__); \
    fprintf(stderr, MSG, ##__VA_ARGS__);                                  \
    fprintf(stderr, "\n");                                                \
  }
#else
  #define DUMP(LABEL, MSG, ...)                                           \
  {                                                                       \
    fprintf(stderr, "%s%s:%d in %s ", LABEL, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
    fprintf(stderr, MSG, ##__VA_ARGS__);                                  \
    fprintf(stderr, "\n");                                                \
  }
#endif

// macro to write a simple message, only works if 'VERBOSE' is #defined
#ifdef VERBOSE
#  define MESSAGE(MSG, ...) DUMP("", MSG, ##__VA_ARGS__)
#else
#  define MESSAGE(MSG, ...)
#endif

// macro to dump errors to stderr if the given condition is true
#define ERROR_IF(CONDITION, MSG, ...) if(CONDITION) { DUMP("ERROR : ", MSG, ##__VA_ARGS__);}

// macro to dump errors to stderr and abort if the given condition is true
#define ERROR_ABORT_IF(CONDITION, MSG, ...)     \
{                                               \
  if(CONDITION) {                               \
    DUMP("FATAL ERROR : ", MSG, ##__VA_ARGS__); \
    abort();                                    \
  }                                             \
}

// anonymous namespace to hide our symbols in
namespace {
  ////////////////////////////////////////////////////////////////////////////////
  // set of suite pointers provided by the host
  OfxHost               *gHost;
  OfxPropertySuiteV1    *gPropertySuite = 0;
  OfxImageEffectSuiteV1 *gImageEffectSuite = 0;

  ////////////////////////////////////////////////////////////////////////////////
  // The first _action_ called after the binary is loaded (three boot strapper functions will be howeever)
  OfxStatus LoadAction(void)
  {
    /// now fetch a suite out of the host via it's fetch suite function.
    gPropertySuite = (OfxPropertySuiteV1 *) gHost->fetchSuite(gHost->host, kOfxPropertySuite, 1);
    ERROR_ABORT_IF(gPropertySuite == 0, "Failed to fetch the " kOfxPropertySuite " version 1 from the host.");

    gImageEffectSuite = (OfxImageEffectSuiteV1 *) gHost->fetchSuite(gHost->host, kOfxImageEffectSuite, 1);
    ERROR_ABORT_IF(gImageEffectSuite == 0, "Failed to fetch the " kOfxImageEffectSuite " version 1 from the host.");

    return kOfxStatOK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // the plugin's basic description routine
  OfxStatus DescribeAction(OfxImageEffectHandle descriptor)
  {
    // get the property set handle for the plugin
    OfxPropertySetHandle effectProps;
    gImageEffectSuite->getPropertySet(descriptor, &effectProps);

    // set some labels and the group it belongs to
    gPropertySuite->propSetString(effectProps,
                                  kOfxPropLabel,
                                  0,
                                  "OFX Invert Example");
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPluginPropGrouping,
                                  0,
                                  "OFX Example");

    // define the image effects contexts we can be used in, in this case a simple filter
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPropSupportedContexts,
                                  0,
                                  kOfxImageEffectContextFilter);

    // set the bit depths the plugin can handle
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPropSupportedPixelDepths,
                                  0,
                                  kOfxBitDepthFloat);
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPropSupportedPixelDepths,
                                  1,
                                  kOfxBitDepthShort);
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPropSupportedPixelDepths,
                                  2,
                                  kOfxBitDepthByte);

    // say that a single instance of this plugin can be rendered in multiple threads
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPluginRenderThreadSafety,
                                  0,
                                  kOfxImageEffectRenderFullySafe);

    // say that the host should manage SMP threading over a single frame
    gPropertySuite->propSetInt(effectProps,
                               kOfxImageEffectPluginPropHostFrameThreading,
                               0,
                               1);

    return kOfxStatOK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  //  describe the plugin in context
  OfxStatus
  DescribeInContextAction(OfxImageEffectHandle descriptor,
                          OfxPropertySetHandle inArgs)
  {
    OfxPropertySetHandle props;
    // define the mandated single output clip
    gImageEffectSuite->clipDefine(descriptor, "Output", &props);

    // set the component types we can handle on out output
    gPropertySuite->propSetString(props,
                                  kOfxImageEffectPropSupportedComponents,
                                  0,
                                  kOfxImageComponentRGBA);
    gPropertySuite->propSetString(props,
                                  kOfxImageEffectPropSupportedComponents,
                                  1,
                                  kOfxImageComponentAlpha);
    gPropertySuite->propSetString(props,
                                  kOfxImageEffectPropSupportedComponents,
                                  2,
                                  kOfxImageComponentRGB);

    // define the mandated single source clip
    gImageEffectSuite->clipDefine(descriptor, "Source", &props);

    // set the component types we can handle on our main input
    gPropertySuite->propSetString(props,
                                  kOfxImageEffectPropSupportedComponents,
                                  0,
                                  kOfxImageComponentRGBA);
    gPropertySuite->propSetString(props,
                                  kOfxImageEffectPropSupportedComponents,
                                  1,
                                  kOfxImageComponentAlpha);
    gPropertySuite->propSetString(props,
                                  kOfxImageEffectPropSupportedComponents,
                                  2,
                                  kOfxImageComponentRGB);

    return kOfxStatOK;
  }


  // Look up a pixel in the image. returns null if the pixel was not
  // in the bounds of the image
  template <class T>
  static inline T * pixelAddress(int x, int y,
                                 void *baseAddress,
                                 OfxRectI bounds,
                                 int rowBytes,
                                 int nCompsPerPixel)
  {
    // Inside the bounds of this image?
    if(x < bounds.x1 || x >= bounds.x2 || y < bounds.y1 || y >= bounds.y2)
      return NULL;

    // turn image plane coordinates into offsets from the bottom left
    int yOffset = y - bounds.y1;
    int xOffset = x - bounds.x1;

    // Find the start of our row, using byte arithmetic
    void *rowStartAsVoid = reinterpret_cast<char *>(baseAddress) + yOffset * rowBytes;

    // turn the row start into a pointer to our data type
    T *rowStart = reinterpret_cast<T *>(rowStartAsVoid);

    // finally find the position of the first component of column
    return rowStart + (xOffset * nCompsPerPixel);
  }

  // iterate over our pixels and process them
  template <class T, int MAX>
  void PixelProcessing(OfxImageEffectHandle instance,
                       OfxPropertySetHandle sourceImg,
                       OfxPropertySetHandle outputImg,
                       OfxRectI renderWindow,
                       int nComps)
  {
    // fetch output image info from the property handle
    int dstRowBytes;
    OfxRectI dstBounds;
    void *dstPtr = NULL;
    gPropertySuite->propGetInt(outputImg, kOfxImagePropRowBytes, 0, &dstRowBytes);
    gPropertySuite->propGetIntN(outputImg, kOfxImagePropBounds, 4, &dstBounds.x1);
    gPropertySuite->propGetPointer(outputImg, kOfxImagePropData, 0, &dstPtr);

    if(dstPtr == NULL) {
      throw "Bad destination pointer";
    }

    // fetch input image info from the property handle
    int srcRowBytes;
    OfxRectI srcBounds;
    void *srcPtr = NULL;
    gPropertySuite->propGetInt(sourceImg, kOfxImagePropRowBytes, 0, &srcRowBytes);
    gPropertySuite->propGetIntN(sourceImg, kOfxImagePropBounds, 4, &srcBounds.x1);
    gPropertySuite->propGetPointer(sourceImg, kOfxImagePropData, 0, &srcPtr);

    if(srcPtr == NULL) {
      throw "Bad source pointer";
    }

    // and do some inverting
    for(int y = renderWindow.y1; y < renderWindow.y2; y++) {
      if(y % 20 == 0 && gImageEffectSuite->abort(instance)) break;

      // get the row start for the output image
      T *dstPix = pixelAddress<T>(renderWindow.x1, y, dstPtr, dstBounds, dstRowBytes, nComps);

      for(int x = renderWindow.x1; x < renderWindow.x2; x++) {

        // get the source pixel
        T *srcPix = pixelAddress<T>(x, y, srcPtr, srcBounds, srcRowBytes, nComps);

        if(srcPix) {
          // we have one, iterate each component in the pixels
          for(int i = 0; i < nComps; ++i) {
            if(i != 3) { // We don't invert alpha.
              *dstPix = MAX - *srcPix; // invert
            }
            else {
              *dstPix = *srcPix;
            }
            ++dstPix; ++srcPix;
          }
        }
        else {
          // we don't have a pixel in the source image, set output to black
          for(int i = 0; i < nComps; ++i) {
            *dstPix = 0;
            ++dstPix;
          }
        }
      }
    }
  }


  ////////////////////////////////////////////////////////////////////////////////
  // Render an output image
  OfxStatus RenderAction( OfxImageEffectHandle instance,
                          OfxPropertySetHandle inArgs,
                          OfxPropertySetHandle outArgs)
  {
    // get the render window and the time from the inArgs
    OfxTime time;
    OfxRectI renderWindow;
    OfxStatus status = kOfxStatOK;

    gPropertySuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);
    gPropertySuite->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4, &renderWindow.x1);

    // fetch output clip
    OfxImageClipHandle outputClip;
    gImageEffectSuite->clipGetHandle(instance, "Output", &outputClip, NULL);

    // fetch main input clip
    OfxImageClipHandle sourceClip;
    gImageEffectSuite->clipGetHandle(instance, "Source", &sourceClip, NULL);

    // the property sets holding our images
    OfxPropertySetHandle outputImg = NULL, sourceImg = NULL;
    try {
      // fetch image to render into from that clip
      OfxPropertySetHandle outputImg;
      if(gImageEffectSuite->clipGetImage(outputClip, time, NULL, &outputImg) != kOfxStatOK) {
        throw " no output image!";
      }

      // fetch image at render time from that clip
      if (gImageEffectSuite->clipGetImage(sourceClip, time, NULL, &sourceImg) != kOfxStatOK) {
        throw " no source image!";
      }

      // figure out the data types
      char *cstr;
      gPropertySuite->propGetString(outputImg, kOfxImageEffectPropComponents, 0, &cstr);
      std::string components = cstr;

      // how many components per pixel?
      int nComps = 0;
      if(components == kOfxImageComponentRGBA) {
        nComps = 4;
      }
      else if(components == kOfxImageComponentRGB) {
        nComps = 3;
      }
      else if(components == kOfxImageComponentAlpha) {
        nComps = 1;
      }
      else {
        throw " bad pixel type!";
      }

      // now do our render depending on the data type
      gPropertySuite->propGetString(outputImg, kOfxImageEffectPropPixelDepth, 0, &cstr);
      std::string dataType = cstr;

      if(dataType == kOfxBitDepthByte) {
        PixelProcessing<unsigned char, 255>(instance, sourceImg, outputImg, renderWindow, nComps);
      }
      else if(dataType == kOfxBitDepthShort) {
        PixelProcessing<unsigned short, 65535>(instance, sourceImg, outputImg, renderWindow, nComps);
      }
      else if (dataType == kOfxBitDepthFloat) {
        PixelProcessing<float, 1>(instance, sourceImg, outputImg, renderWindow, nComps);
      }
      else {
        throw " bad data type!";
        throw 1;
      }

    }
    catch(const char *errStr ) {
      bool isAborting = gImageEffectSuite->abort(instance);

      // if we were interrupted, the failed fetch is fine, just return kOfxStatOK
      // otherwise, something weird happened
      if(!isAborting) {
        status = kOfxStatFailed;
      }
      ERROR_IF(!isAborting, " Rendering failed because %s", errStr);

    }

    if(sourceImg)
      gImageEffectSuite->clipReleaseImage(sourceImg);
    if(outputImg)
      gImageEffectSuite->clipReleaseImage(outputImg);

    // all was well
    return status;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Call back passed to the host in the OfxPlugin struct to set our host pointer
  //
  // This must be called AFTER both OfxGetNumberOfPlugins and OfxGetPlugin, but
  // before the pluginMain entry point is ever touched.
  void SetHostFunc(OfxHost *hostStruct)
  {
    gHost = hostStruct;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // The main entry point function, the host calls this to get the plugin to do things.
  OfxStatus MainEntryPoint(const char *action, const void *handle, OfxPropertySetHandle inArgs,  OfxPropertySetHandle outArgs)
  {
    MESSAGE(": START action is : %s \n", action );
    // cast to appropriate type
    OfxImageEffectHandle effect = (OfxImageEffectHandle) handle;

    OfxStatus returnStatus = kOfxStatReplyDefault;

    if(strcmp(action, kOfxActionLoad) == 0) {
      // The very first action called on a plugin.
      returnStatus = LoadAction();
    }
    else if(strcmp(action, kOfxActionDescribe) == 0) {
      // the first action called to describe what the plugin does
      returnStatus = DescribeAction(effect);
    }
    else if(strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
      // the second action called to describe what the plugin does in a specific context
      returnStatus = DescribeInContextAction(effect, inArgs);
    }
    else if(strcmp(action, kOfxImageEffectActionRender) == 0) {
      // action called to render a frame
      returnStatus = RenderAction(effect, inArgs, outArgs);
    }

    MESSAGE(": END action is : %s \n", action );
    /// other actions to take the default value
    return returnStatus;
  }

} // end of anonymous namespace


////////////////////////////////////////////////////////////////////////////////
// The plugin struct passed back to the host application to initiate bootstrapping\
// of plugin communications
static OfxPlugin effectPluginStruct =
{
  kOfxImageEffectPluginApi,                // The API this plugin satisfies.
  1,                                       // The version of the API it satisifes.
  "org.openeffects:InvertExamplePlugin",   // The unique ID of this plugin.
  1,                                       // The major version number of this plugin.
  0,                                       // The minor version number of this plugin.
  SetHostFunc,                             // Function used to pass back to the plugin the OFXHost struct.
  MainEntryPoint                           // The main entry point to the plugin where all actions are passed to.
};

////////////////////////////////////////////////////////////////////////////////
// The first of the two functions that a host application will look for
// after loading the binary, this function returns the number of plugins within
// this binary.
//
// This will be the first function called by the host.
EXPORT int OfxGetNumberOfPlugins(void)
{
  return 1;
}

////////////////////////////////////////////////////////////////////////////////
// The second of the two functions that a host application will look for
// after loading the binary, this function returns the 'nth' plugin declared in
// this binary.
//
// This will be called multiple times by the host, once for each plugin present.
EXPORT OfxPlugin * OfxGetPlugin(int nth)
{
  if(nth == 0)
    return &effectPluginStruct;
  return 0;
}
