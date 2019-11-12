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

  This plugin will take you through the basics of defining and using
  parameters as well as how to use instance data.

  The accompanying guide will explain what is happening in more detail.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
//#define VERBOSE
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

// name of our params
#define RADIUS_PARAM_NAME "radius"
#define CENTRE_PARAM_NAME "centre"
#define COLOUR_PARAM_NAME "colour"
#define GROW_ROD_PARAM_NAME "growRoD"

// anonymous namespace to hide our symbols in
namespace {
  ////////////////////////////////////////////////////////////////////////////////
  // set of suite pointers provided by the host
  OfxHost               *gHost;
  OfxPropertySuiteV1    *gPropertySuite    = 0;
  OfxImageEffectSuiteV1 *gImageEffectSuite = 0;
  OfxParameterSuiteV1   *gParameterSuite   = 0;

  // version of the API the host is running
  int gAPIVersion[2] = {1, 0};

  // does the host support multi resolution images
  int gHostSupportsMultiRes = false;

  ////////////////////////////////////////////////////////////////////////////////
  // class to manage OFX images
  class Image {
  public    :
    // construct from a property set that represents the image
    Image(OfxPropertySetHandle propSet);

    // construct from a clip by fetching an image at the given frame
    Image(OfxImageClipHandle clip, double frame);

    // destructor
    ~Image();

    // get a pixel address, cast to the right type
    template <class T>
    T *pixelAddress(int x, int y)
    {
      return reinterpret_cast<T *>(rawAddress(x, y));
    }

    // get a pixel address, if it doesn't exist
    // return a default black pixel
    template <class T>
    const T *pixelAddressWithFallback(int x, int y)
    {
      const T *pix = pixelAddress<T>(x, y);
      if(!pix) {
        static const T blackPix[4] = {0,0,0,0};
        pix = blackPix;
      }
      return pix;
    }

    // Is this image empty?
    operator bool();

    // bytes per component, 1, 2 or 4 for byte, short and float images
    int bytesPerComponent() const { return bytesPerComponent_; }

    // number of components
    int nComponents() const { return nComponents_; }

    // number of components
    double pixelAspectRatio() const { return pixelAspectRatio_; }

  protected :
    void construct();

    // Look up a pixel address in the image. returns null if the pixel was not
    // in the bounds of the image
    void *rawAddress(int x, int y);

    OfxPropertySetHandle propSet_;
    int rowBytes_;
    OfxRectI bounds_;
    char *dataPtr_;
    int nComponents_;
    int bytesPerComponent_;
    int bytesPerPixel_;
    double pixelAspectRatio_;
  };

  // construct from a property set
  Image::Image(OfxPropertySetHandle propSet)
    : propSet_(propSet)
  {
    construct();
  }

  // construct by fetching from a clip
  Image::Image(OfxImageClipHandle clip, double time)
    : propSet_(NULL)
  {
    if (clip && (gImageEffectSuite->clipGetImage(clip, time, NULL, &propSet_) == kOfxStatOK)) {
      construct();
    }
    else {
      propSet_ = NULL;
    }
  }

  // assemble it all together
  void Image::construct()
  {
    if(propSet_) {
      gPropertySuite->propGetInt(propSet_, kOfxImagePropRowBytes, 0, &rowBytes_);
      gPropertySuite->propGetIntN(propSet_, kOfxImagePropBounds, 4, &bounds_.x1);
      gPropertySuite->propGetPointer(propSet_, kOfxImagePropData, 0, (void **) &dataPtr_);
      gPropertySuite->propGetDouble(propSet_, kOfxImagePropPixelAspectRatio, 0, &pixelAspectRatio_);

      // how many components per pixel?
      char *cstr;
      gPropertySuite->propGetString(propSet_, kOfxImageEffectPropComponents, 0, &cstr);

      if(strcmp(cstr, kOfxImageComponentRGBA) == 0) {
        nComponents_ = 4;
      }
      else if(strcmp(cstr, kOfxImageComponentRGB) == 0) {
        nComponents_ = 3;
      }
      else if(strcmp(cstr, kOfxImageComponentAlpha) == 0) {
        nComponents_ = 1;
      }
      else {
        throw " bad pixel type!";
      }

      // what is the data type
      gPropertySuite->propGetString(propSet_, kOfxImageEffectPropPixelDepth, 0, &cstr);
      if(strcmp(cstr, kOfxBitDepthByte) == 0) {
        bytesPerComponent_ = 1;
      }
      else if(strcmp(cstr, kOfxBitDepthShort) == 0) {
        bytesPerComponent_ = 2;
      }
      else if(strcmp(cstr, kOfxBitDepthFloat) == 0) {
        bytesPerComponent_ = 4;
      }
      else {
        throw " bad pixel type!";
      }

      bytesPerPixel_ = bytesPerComponent_ * nComponents_;
    }
    else {
      rowBytes_ = 0;
      bounds_.x1 = bounds_.x2 = bounds_.y1 = bounds_.y2 = 0;
      dataPtr_ = NULL;
      nComponents_ = 0;
      bytesPerComponent_ = 0;
    }
  }

  // destructor
  Image::~Image()
  {
    if(propSet_)
      gImageEffectSuite->clipReleaseImage(propSet_);
  }

  // get the address of a location in the image as a void *
  void *Image::rawAddress(int x, int y)
  {
    // Inside the bounds of this image?
    if(x < bounds_.x1 || x >= bounds_.x2 || y < bounds_.y1 || y >= bounds_.y2)
      return NULL;

    // turn image plane coordinates into offsets from the bottom left
    int yOffset = y - bounds_.y1;
    int xOffset = x - bounds_.x1;

    // Find the start of our row, using byte arithmetic
    char *rowStart = (dataPtr_) + yOffset * rowBytes_;

    // finally find the position of the first component of column
    return rowStart + (xOffset * bytesPerPixel_);
  }

  // are we empty?
  Image:: operator bool()
  {
    return propSet_ != NULL && dataPtr_ != NULL;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // our instance data, where we are caching away clip and param handles
  struct MyInstanceData {
    // are we in the general context
    bool isGeneralContext;

    // handles to the clips we deal with
    OfxImageClipHandle sourceClip;
    OfxImageClipHandle outputClip;

    // handles to a our parameters
    OfxParamHandle centreParam;
    OfxParamHandle radiusParam;
    OfxParamHandle colourParam;
    OfxParamHandle growRoD;

    MyInstanceData()
      : isGeneralContext(false)
      , sourceClip(NULL)
      , outputClip(NULL)
      , centreParam(NULL)
      , radiusParam(NULL)
      , colourParam(NULL)
      , growRoD(NULL)
    {}
  };

  ////////////////////////////////////////////////////////////////////////////////
  // get my instance data from a property set handle
  MyInstanceData *FetchInstanceData(OfxPropertySetHandle effectProps)
  {
    MyInstanceData *myData = 0;
    gPropertySuite->propGetPointer(effectProps,
                                   kOfxPropInstanceData,
                                   0,
                                   (void **) &myData);
    return myData;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // get my instance data
  MyInstanceData *FetchInstanceData(OfxImageEffectHandle effect)
  {
    // get the property handle for the plugin
    OfxPropertySetHandle effectProps;
    gImageEffectSuite->getPropertySet(effect, &effectProps);

    // and get the instance data out of that
    return FetchInstanceData(effectProps);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // get the named suite and put it in the given pointer, with error checking
  template <class SUITE>
  void FetchSuite(SUITE *& suite, const char *suiteName, int suiteVersion)
  {
    suite = (SUITE *) gHost->fetchSuite(gHost->host, suiteName, suiteVersion);
    if(!suite) {
      ERROR_ABORT_IF(suite == NULL,
                     "Failed to fetch %s version %d from the host.",
                     suiteName,
                     suiteVersion);
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  // The first _action_ called after the binary is loaded (three boot strapper functions will be howeever)
  OfxStatus LoadAction(void)
  {
    // fetch our three suites
    FetchSuite(gPropertySuite,    kOfxPropertySuite,    1);
    FetchSuite(gImageEffectSuite, kOfxImageEffectSuite, 1);
    FetchSuite(gParameterSuite,   kOfxParameterSuite,   1);

    int verSize = 0;
    if(gPropertySuite->propGetDimension(gHost->host, kOfxPropAPIVersion, &verSize) == kOfxStatOK) {
      verSize = verSize > 2 ? 2 : verSize;
      gPropertySuite->propGetIntN(gHost->host,
                                  kOfxPropAPIVersion,
                                  2,
                                  gAPIVersion);
    }

    // we only support 1.2 and above
    if(gAPIVersion[0] == 1 && gAPIVersion[1] < 2) {
      return kOfxStatFailed;
    }

    /// does the host support multi-resolution images
    gPropertySuite->propGetInt(gHost->host,
                               kOfxImageEffectPropSupportsMultiResolution,
                               0,
                               &gHostSupportsMultiRes);

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
                                  "OFX Circle Example");
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPluginPropGrouping,
                                  0,
                                  "OFX Example");

    // define the image effects contexts we can be used in, in this case a filter
    // and a general effect
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
    // get the context we are being described for
    char *context;
    gPropertySuite->propGetString(inArgs, kOfxImageEffectPropContext, 0, &context);

    // what components do we support
    static const char *supportedComponents[] = {kOfxImageComponentRGBA, kOfxImageComponentRGB, kOfxImageComponentAlpha};

    OfxPropertySetHandle props;
    // define the mandated single output clip
    gImageEffectSuite->clipDefine(descriptor, "Output", &props);

    // set the component types we can handle on out output
    gPropertySuite->propSetStringN(props,
                                   kOfxImageEffectPropSupportedComponents,
                                   3,
                                   supportedComponents);

    // define the mandated single source clip
    gImageEffectSuite->clipDefine(descriptor, "Source", &props);

    // set the component types we can handle on our main input
    gPropertySuite->propSetStringN(props,
                                   kOfxImageEffectPropSupportedComponents,
                                   3,
                                   supportedComponents);

    // first get the handle to the parameter set
    OfxParamSetHandle paramSet;
    gImageEffectSuite->getParamSet(descriptor, &paramSet);

    // properties on our parameter
    OfxPropertySetHandle radiusParamProps;

    // set the properties on the radius param
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeDouble,
                                 RADIUS_PARAM_NAME,
                                 &radiusParamProps);

    gPropertySuite->propSetString(radiusParamProps,
                                  kOfxParamPropDoubleType,
                                  0,
                                  kOfxParamDoubleTypeX);

    gPropertySuite->propSetString(radiusParamProps,
                                  kOfxParamPropDefaultCoordinateSystem,
                                  0,
                                  kOfxParamCoordinatesNormalised);

    gPropertySuite->propSetDouble(radiusParamProps,
                                  kOfxParamPropDefault,
                                  0,
                                  0.25);
    gPropertySuite->propSetDouble(radiusParamProps,
                                  kOfxParamPropMin,
                                  0,
                                  0);
    gPropertySuite->propSetDouble(radiusParamProps,
                                  kOfxParamPropDisplayMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(radiusParamProps,
                                  kOfxParamPropDisplayMax,
                                  0,
                                  2.0);
    gPropertySuite->propSetString(radiusParamProps,
                                  kOfxPropLabel,
                                  0,
                                  "Radius");
    gPropertySuite->propSetString(radiusParamProps,
                                  kOfxParamPropHint,
                                  0,
                                  "The radius of the circle.");

    // set the properties on the centre param
    OfxPropertySetHandle centreParamProps;
    static double centreDefault[] = {0.5, 0.5};

    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeDouble2D,
                                 CENTRE_PARAM_NAME,
                                 &centreParamProps);

    gPropertySuite->propSetString(centreParamProps,
                                  kOfxParamPropDoubleType,
                                  0,
                                  kOfxParamDoubleTypeXYAbsolute);
    gPropertySuite->propSetString(centreParamProps,
                                  kOfxParamPropDefaultCoordinateSystem,
                                  0,
                                  kOfxParamCoordinatesNormalised);
    gPropertySuite->propSetDoubleN(centreParamProps,
                                   kOfxParamPropDefault,
                                   2,
                                   centreDefault);
    gPropertySuite->propSetString(centreParamProps,
                                  kOfxPropLabel,
                                  0,
                                  "Centre");
    gPropertySuite->propSetString(centreParamProps,
                                  kOfxParamPropHint,
                                  0,
                                  "The centre of the circle.");


    // set the properties on the colour param
    OfxPropertySetHandle colourParamProps;
    static double colourDefault[] = {1.0, 1.0, 1.0, 0.5};

    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeRGBA,
                                 COLOUR_PARAM_NAME,
                                 &colourParamProps);
    gPropertySuite->propSetDoubleN(colourParamProps,
                                   kOfxParamPropDefault,
                                   4,
                                   colourDefault);
    gPropertySuite->propSetString(colourParamProps,
                                  kOfxPropLabel,
                                  0,
                                  "Colour");
    gPropertySuite->propSetString(centreParamProps,
                                  kOfxParamPropHint,
                                  0,
                                  "The colour of the circle.");

    // and define the 'grow RoD' parameter and set its properties
    if(gHostSupportsMultiRes) {
      OfxPropertySetHandle growRoDParamProps;
      gParameterSuite->paramDefine(paramSet,
                                   kOfxParamTypeBoolean,
                                   GROW_ROD_PARAM_NAME,
                                   &growRoDParamProps);
      gPropertySuite->propSetInt(growRoDParamProps,
                                 kOfxParamPropDefault,
                                 0,
                                 0);
      gPropertySuite->propSetString(growRoDParamProps,
                                    kOfxParamPropHint,
                                    0,
                                    "Whether to grow the output's Region of Definition to include the circle.");
      gPropertySuite->propSetString(growRoDParamProps,
                                    kOfxPropLabel,
                                    0,
                                    "Grow RoD");
    }

    return kOfxStatOK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// instance construction
  OfxStatus CreateInstanceAction( OfxImageEffectHandle instance)
  {
    OfxPropertySetHandle effectProps;
    gImageEffectSuite->getPropertySet(instance, &effectProps);

    // To avoid continual lookup, put our handles into our instance
    // data, those handles are guaranteed to be valid for the duration
    // of the instance.
    MyInstanceData *myData = new MyInstanceData;

    // Set my private instance data
    gPropertySuite->propSetPointer(effectProps, kOfxPropInstanceData, 0, (void *) myData);

    // is this instance made for the general context?
    char *context = 0;
    gPropertySuite->propGetString(effectProps, kOfxImageEffectPropContext, 0,  &context);
    myData->isGeneralContext = context && (strcmp(context, kOfxImageEffectContextGeneral) == 0);

    // Cache the source and output clip handles
    gImageEffectSuite->clipGetHandle(instance, "Source", &myData->sourceClip, 0);
    gImageEffectSuite->clipGetHandle(instance, "Output", &myData->outputClip, 0);

    // Cache away the param handles
    OfxParamSetHandle paramSet;
    gImageEffectSuite->getParamSet(instance, &paramSet);
    gParameterSuite->paramGetHandle(paramSet,
                                    RADIUS_PARAM_NAME,
                                    &myData->radiusParam,
                                    0);
    gParameterSuite->paramGetHandle(paramSet,
                                    CENTRE_PARAM_NAME,
                                    &myData->centreParam,
                                    0);
    gParameterSuite->paramGetHandle(paramSet,
                                    COLOUR_PARAM_NAME,
                                    &myData->colourParam,
                                    0);

    if(gHostSupportsMultiRes) {
      gParameterSuite->paramGetHandle(paramSet,
                                      GROW_ROD_PARAM_NAME,
                                      &myData->growRoD,
                                      0);
    }

    return kOfxStatOK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // instance destruction
  OfxStatus DestroyInstanceAction( OfxImageEffectHandle instance)
  {
    // get my instance data
    MyInstanceData *myData = FetchInstanceData(instance);
    delete myData;

    return kOfxStatOK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // clamp to 0 and MAX inclusive
  template <class T, int MAX>
  static inline T Clamp(float value)
  {
    if(MAX == 1)
      return value; // don't clamp floating point values
    else
      return value < 0 ? T(0) : (value > MAX ? T(MAX) : T(value));
  }

  ////////////////////////////////////////////////////////////////////////////////
  // clamp to 0 and MAX inclusive
  template <class T1, class T2>
  static inline T1 Blend(T1 v1, T2 v2, float blend)
  {
    return v1 + (v2-v1) * blend;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // iterate over our pixels and process them
  template <class T, int MAX>
  void PixelProcessing(OfxImageEffectHandle instance,
                       Image &src,
                       Image &output,
                       double centre[2],
                       double radius,
                       double colour[4],
                       double renderScale[2],
                       OfxRectI renderWindow)
  {
    // pixel aspect of our output
    float PAR = output.pixelAspectRatio();

    T colourQuantised[4];
    for(int c = 0; c < 4; ++c) {
      colourQuantised[c] = Clamp<T, MAX>(colour[c] * MAX);
    }

    // now do some processing
    for(int y = renderWindow.y1; y < renderWindow.y2; y++) {
      if( y % 20 == 0 && gImageEffectSuite->abort(instance)) break;

      // get our y coord in canonical space
      float yCanonical = (y + 0.5f)/renderScale[1];

      // how far are we from the centre in y, canonical
      float dy = yCanonical - centre[1];

      // get the row start for the output image
      T *dstPix = output.pixelAddress<T>(renderWindow.x1, y);

      for(int x = renderWindow.x1; x < renderWindow.x2; x++) {
        // get our x pixel coord in canonical space,
        float xCanonical = (x + 0.5) * PAR/renderScale[0];

        // how far are we from the centre in x, canonical
        float dx = xCanonical - centre[0];

        // distance to the centre of our circle, canonical
        float d = sqrtf(dx * dx + dy * dy);

        // this will hold the antialiased value
        float alpha = colour[3];

        // Is the square of the distance to the centre
        // less than the square of the radius?
        if(d < radius) {
          if(d > radius - 1) {
            // we are within 1 pixel of the edge, modulate
            // our alpha with an anti-aliasing value
            alpha *= radius - d;
          }
        }
        else {
          // outside, so alpha is 0
          alpha = 0;
        }

        // get the source pixel
        const T *srcPix = src.pixelAddressWithFallback<T>(x, y);

        // scale each component around that average
        for(int c = 0; c < output.nComponents(); ++c) {
          // use the mask to control how much original we should have
          dstPix[c] = Blend(srcPix[c], colourQuantised[c], alpha);
        }
        dstPix += output.nComponents();
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
    double renderScale[2];
    OfxStatus status = kOfxStatOK;

    gPropertySuite->propGetDouble(inArgs,
                                  kOfxPropTime,
                                  0,
                                  &time);
    gPropertySuite->propGetIntN(inArgs,
                                kOfxImageEffectPropRenderWindow,
                                4,
                                &renderWindow.x1);
    gPropertySuite->propGetDoubleN(inArgs,
                                   kOfxImageEffectPropRenderScale,
                                   2,
                                   renderScale);

    // get our instance data which has out clip and param handles
    MyInstanceData *myData = FetchInstanceData(instance);

    // get our param values
    double radius = 0.0;
    gParameterSuite->paramGetValueAtTime(myData->radiusParam, time, &radius);
    double centre[2];
    gParameterSuite->paramGetValueAtTime(myData->centreParam, time, &centre[0], &centre[1]);
    double colour[4];
    gParameterSuite->paramGetValueAtTime(myData->colourParam, time, &colour[0], &colour[1], &colour[2], &colour[3]);

    // the property sets holding our images
    OfxPropertySetHandle outputImg = NULL, sourceImg = NULL;
    try {
      // fetch image to render into from that clip
      Image outputImg(myData->outputClip, time);
      if(!outputImg) {
        throw " no output image!";
      }

      // fetch image to render into from that clip
      Image sourceImg(myData->sourceClip, time);
      if(!sourceImg) {
        throw " no source image!";
      }

      // now do our render depending on the data type
      if(outputImg.bytesPerComponent() == 1) {
        PixelProcessing<unsigned char, 255>(instance,
                                            sourceImg,
                                            outputImg,
                                            centre,
                                            radius,
                                            colour,
                                            renderScale,
                                            renderWindow);
      }
      else if(outputImg.bytesPerComponent() == 2) {
        PixelProcessing<unsigned short, 65535>(instance,
                                               sourceImg,
                                               outputImg,
                                               centre,
                                               radius,
                                               colour,
                                               renderScale,
                                               renderWindow);
      }
      else if(outputImg.bytesPerComponent() == 4) {
        PixelProcessing<float, 1>(instance,
                                  sourceImg,
                                  outputImg,
                                  centre,
                                  radius,
                                  colour,
                                  renderScale,
                                  renderWindow);
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
    // all was well
    return status;
  }

  // tells the host what region we are capable of filling
  OfxStatus
  GetRegionOfDefinitionAction( OfxImageEffectHandle  effect,
                               OfxPropertySetHandle inArgs,
                               OfxPropertySetHandle outArgs)
  {
    // retrieve any instance data associated with this effect
    MyInstanceData *myData = FetchInstanceData(effect);

    OfxTime time;
    gPropertySuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);

    int growingRoD;
    gParameterSuite->paramGetValueAtTime(myData->growRoD, time,
                                         &growingRoD);

    // are we growing the RoD to include the circle?
    if (!growingRoD) {
      return kOfxStatReplyDefault;
    }
    else {
      double radius = 0.0;
      gParameterSuite->paramGetValueAtTime(myData->radiusParam, time,
                                           &radius);

      double centre[2];
      gParameterSuite->paramGetValueAtTime(myData->centreParam, time,
                                           &centre[0],
                                           &centre[1]);

      // get the source rod
      OfxRectD rod;
      gImageEffectSuite->clipGetRegionOfDefinition(myData->sourceClip, time, &rod);

      if(rod.x1 > centre[0] - radius) rod.x1 = centre[0] - radius;
      if(rod.y1 > centre[1] - radius) rod.y1 = centre[1] - radius;

      if(rod.x2 < centre[0] + radius) rod.x2 = centre[0] + radius;
      if(rod.y2 < centre[1] + radius) rod.y2 = centre[1] + radius;

      // set the rod in the out args
      gPropertySuite->propSetDoubleN(outArgs, kOfxImageEffectPropRegionOfDefinition, 4, &rod.x1);

      // and say we trapped the action and we are at the identity
      return kOfxStatOK;
    }
  }

  // are the settings of the effect making it redundant and so not do anything to the image data
  OfxStatus IsIdentityAction( OfxImageEffectHandle instance,
                              OfxPropertySetHandle inArgs,
                              OfxPropertySetHandle outArgs)
  {
    MyInstanceData *myData = FetchInstanceData(instance);

    bool isIdentity = false;

    double time;
    gPropertySuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);

    double radius = 0.0;
    gParameterSuite->paramGetValueAtTime(myData->radiusParam, time, &radius);

    // if the radius is zero then we don't draw anything and it has no effect
    isIdentity = radius < 0.0001;

    int growingRoD = 0;
    if(gHostSupportsMultiRes) {
      gParameterSuite->paramGetValueAtTime(myData->growRoD, time, &growingRoD);
    }

    // if we are drawing out side of the RoD and we aren't growing to include it, we have no effect
    if(!isIdentity && !growingRoD) {
      OfxRectD bounds;
      double centre[2];
      gParameterSuite->paramGetValueAtTime(myData->centreParam, time, &centre[0], &centre[1]);

      gImageEffectSuite->clipGetRegionOfDefinition(myData->sourceClip, time, &bounds);

      isIdentity = (centre[0] + radius < bounds.x1 ||
                    centre[0] - radius > bounds.x2 ||
                    centre[1] + radius < bounds.y1 ||
                    centre[1] - radius > bounds.y2);
    }


    if(isIdentity) {
      // we set the name of the input clip to pull default images from
      gPropertySuite->propSetString(outArgs, kOfxPropName, 0, "Source");

      // and say we trapped the action and we are at the identity
      return kOfxStatOK;
    }

    // say we aren't at the identity
    return kOfxStatReplyDefault;
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
    else if(strcmp(action, kOfxActionCreateInstance) == 0) {
      // the action called when an instance of a plugin is created
      returnStatus = CreateInstanceAction(effect);
    }
    else if(strcmp(action, kOfxActionDestroyInstance) == 0) {
      // the action called when an instance of a plugin is destroyed
      returnStatus = DestroyInstanceAction(effect);
    }
    else if(strcmp(action, kOfxImageEffectActionIsIdentity) == 0) {
      // Check to see if our param settings cause nothing to happen
      returnStatus = IsIdentityAction(effect, inArgs, outArgs);
    }
    else if(strcmp(action, kOfxImageEffectActionRender) == 0) {
      // action called to render a frame
      returnStatus = RenderAction(effect, inArgs, outArgs);
    }
    else if(gHostSupportsMultiRes && strcmp(action, kOfxImageEffectActionGetRegionOfDefinition) == 0) {
      returnStatus = GetRegionOfDefinitionAction(effect, inArgs, outArgs);
    }


    MESSAGE(": END action is : %s \n", action );
    /// other actions to take the default value
    return returnStatus;
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

} // end of anonymous namespace


////////////////////////////////////////////////////////////////////////////////
// The plugin struct passed back to the host application to initiate bootstrapping\
// of plugin communications
static OfxPlugin effectPluginStruct =
{
  kOfxImageEffectPluginApi,                  // The API this plugin satisfies.
  1,                                         // The version of the API it satisifes.
  "org.openeffects:CircleExamplePlugin",     // The unique ID of this plugin.
  1,                                         // The major version number of this plugin.
  0,                                         // The minor version number of this plugin.
  SetHostFunc,                               // Function used to pass back to the plugin the OFXHost struct.
  MainEntryPoint                             // The main entry point to the plugin where all actions are passed to.
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
