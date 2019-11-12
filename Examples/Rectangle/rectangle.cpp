/*
Software License :

Copyright (c) 2003, The Open Effects Association Ltd. All rights reserved.

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
  Ofx Example plugin that show a plugin that can work as a generator, filter and 
  generic plugin. 

  It draws a rectangle over a background (or just a rectangle if a generator).

  It is meant to illustrate certain features of the API, as opposed to being a perfectly
  crafted piece of image processing software.

  The main features are
    - how generators work,
    - how spatially based effects should scale parameters appropriately.
    - premultiplication
 */
#include <cstring>
#include <stdexcept>
#include <new>
#include "ofxImageEffect.h"
#include "ofxMemory.h"
#include "ofxMultiThread.h"

#include "../include/ofxUtilities.H" // example support utils

#if defined __APPLE__ || defined linux || defined __FreeBSD__
#  define EXPORT __attribute__((visibility("default")))
#elif defined _WIN32
#  define EXPORT OfxExport
#else
#  error Not building on your operating system quite yet
#endif

template <class T> inline T Maximum(T a, T b) {return a > b ? a : b;}
template <class T> inline T Minimum(T a, T b) {return a < b ? a : b;}


// pointers64 to various bits of the host
OfxHost                 *gHost;
OfxImageEffectSuiteV1 *gEffectHost = 0;
OfxPropertySuiteV1    *gPropHost = 0;
OfxParameterSuiteV1   *gParamHost = 0;
OfxMemorySuiteV1      *gMemoryHost = 0;
OfxMultiThreadSuiteV1 *gThreadHost = 0;
OfxMessageSuiteV1 *gMessageSuite = 0;
OfxInteractSuiteV1    *gInteractHost = 0;

// some flags about the host's behaviour
int gHostSupportsMultipleBitDepths = false;

enum ContextEnum {
    eIsGenerator,
    eIsFilter,
    eIsGeneral
};
// private instance data type
struct MyInstanceData {
  ContextEnum context;

  // handles to the clips we deal with
  OfxImageClipHandle sourceClip;
  OfxImageClipHandle outputClip;

  // handles to a our parameters
  OfxParamHandle corner1Param;
  OfxParamHandle corner2Param;
  OfxParamHandle colourParam;
};

/* mandatory function to set up the host structures */


// Convenience wrapper to get private data 
static MyInstanceData *
getMyInstanceData(OfxImageEffectHandle effect)
{
  MyInstanceData *myData = (MyInstanceData *) ofxuGetEffectInstanceData(effect);
  return myData;
}


/** @brief Called at load */

//  instance construction
static OfxStatus
createInstance(OfxImageEffectHandle effect)
{
  // get a pointer to the effect properties
  OfxPropertySetHandle effectProps;
  gEffectHost->getPropertySet(effect, &effectProps);

  // get a pointer to the effect's parameter set
  OfxParamSetHandle paramSet;
  gEffectHost->getParamSet(effect, &paramSet);

  // make my private instance data
  MyInstanceData *myData = new MyInstanceData;
  char *context = 0;

  // is this instance a general effect ?
  gPropHost->propGetString(effectProps, kOfxImageEffectPropContext, 0,  &context);
  if(strcmp(context, kOfxImageEffectContextGenerator) == 0) {
    myData->context = eIsGenerator;
  }
  else if(strcmp(context, kOfxImageEffectContextFilter) == 0) {
    myData->context = eIsFilter;
  }
  else {
    myData->context = eIsGeneral;
  }

  // cache away param handles
  gParamHost->paramGetHandle(paramSet, "corner1", &myData->corner1Param, 0);
  gParamHost->paramGetHandle(paramSet, "corner2", &myData->corner2Param, 0);
  gParamHost->paramGetHandle(paramSet, "colour", &myData->colourParam, 0);

  // cache away clip handles
  if( myData->context  != eIsGenerator)
    gEffectHost->clipGetHandle(effect, kOfxImageEffectSimpleSourceClipName, &myData->sourceClip, 0);
  gEffectHost->clipGetHandle(effect, kOfxImageEffectOutputClipName, &myData->outputClip, 0);
  
  // set my private instance data
  gPropHost->propSetPointer(effectProps, kOfxPropInstanceData, 0, (void *) myData);

  return kOfxStatOK;
}

// instance destruction
static OfxStatus
destroyInstance(OfxImageEffectHandle effect)
{
  // get my instance data
  MyInstanceData *myData = getMyInstanceData(effect);

  // and delete it
  if(myData)
    delete myData;
  return kOfxStatOK;
}

// function that gets the corners params in the canonical coordinate system
static void
getCannonicalRect(OfxImageEffectHandle effect, double time, OfxRectD &rect)
{
  MyInstanceData *myData = getMyInstanceData(effect);
  
  // get  my parameter values
  OfxPointD c1, c2;
  gParamHost->paramGetValueAtTime(myData->corner1Param, time, &c1.x, &c1.y);
  gParamHost->paramGetValueAtTime(myData->corner2Param, time, &c2.x, &c2.y);
  
  // and min/max 'em into the rect
  rect.x1 = Minimum(c1.x, c2.x);
  rect.y1 = Minimum(c1.y, c2.y);
  rect.x2 = Maximum(c1.x, c2.x);
  rect.y2 = Maximum(c1.y, c2.y);
}

// tells the host what region we are capable of filling
OfxStatus 
getSpatialRoD(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs)
{
  // retrieve any instance data associated with this effect
  MyInstanceData *myData = getMyInstanceData(effect);

  OfxTime time;
  gPropHost->propGetDouble(inArgs, kOfxPropTime, 0, &time);

  // get my rectangle in canonical coords, which is my rod
  OfxRectD rod;
  getCannonicalRect(effect, time, rod);

  if(myData->context != eIsGenerator){ 
    // or this with the input RoD
    OfxRectD sourceRoD;
    gEffectHost->clipGetRegionOfDefinition(myData->sourceClip, time, &sourceRoD);
    
    // check to see if the source RoD is infinite in X
    if(ofxuInfiniteRectInX(sourceRoD)) {
      rod.x1 = kOfxFlagInfiniteMin;
      rod.x2 = kOfxFlagInfiniteMax;
    }
    else {
      // find the union of the clip rod and our geometry
      rod.x1 = Minimum(rod.x1, sourceRoD.x1);
      rod.x2 = Maximum(rod.x2, sourceRoD.x2);
    }

    // check to see if the source RoD is infinite in Y
    if(ofxuInfiniteRectInY(sourceRoD)) {
      rod.y1 = kOfxFlagInfiniteMin;
      rod.y2 = kOfxFlagInfiniteMax;
    } 
    else {
      // find the union of the clip rod and our geometry
      rod.y1 = Minimum(rod.y1, sourceRoD.y1);
      rod.y2 = Maximum(rod.y2, sourceRoD.y2);
    }
  }

  // and set the rod in the out args
  gPropHost->propSetDoubleN(outArgs, kOfxImageEffectPropRegionOfDefinition, 4, &rod.x1);

  return kOfxStatOK;
}

// tells the host how much of the input we need to fill the given window
OfxStatus 
getSpatialRoI(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs)
{
  // retrieve any instance data associated with this effect
  MyInstanceData *myData = getMyInstanceData(effect);

  if(myData->context != eIsGenerator){ 
    // get the RoI the effect is interested in from inArgs
    OfxRectD roi;
    gPropHost->propGetDoubleN(inArgs, kOfxImageEffectPropRegionOfInterest, 4, &roi.x1);

    // the input needed is the same as this, so set that on the source clip
    gPropHost->propSetDoubleN(outArgs, "OfxImageClipPropRoI_Source", 4, &roi.x1);
    
    return kOfxStatOK;
  }
  else // we are a generator, return the default
    return  kOfxStatReplyDefault;
}

// are the settings of the effect performing an identity operation
static OfxStatus
isIdentity(OfxImageEffectHandle effect,
	   OfxPropertySetHandle inArgs,
	   OfxPropertySetHandle outArgs)
{
  // retrieve any instance data associated with this effect
  MyInstanceData *myData = getMyInstanceData(effect);
  
  // we should not be called on a generator
  if(myData->context != eIsGenerator){ 

    // get the render window and the time from the inArgs
    OfxTime time;
    OfxRectI renderWindow;
  
    gPropHost->propGetDouble(inArgs, kOfxPropTime, 0, &time);
    gPropHost->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4, &renderWindow.x1);

    // get my rectangle in canonical coords
    OfxRectD rect;
    getCannonicalRect(effect, time, rect);

    OfxRGBAColourD col;
    gParamHost->paramGetValueAtTime(myData->colourParam, time, &col.r, &col.g, &col.b, &col.a);

    // if the rectangle is transparent or out of the window, then we can do a pass through on to the source clip
    if(col.a <= 0.0 || rect.x1 > renderWindow.x2 ||  rect.y1 > renderWindow.y2 || 
       rect.x2 < renderWindow.x1 ||  rect.y2 < renderWindow.y1) {
      // set the property in the out args indicating which is the identity clip
      gPropHost->propSetString(outArgs, kOfxPropName, 0, kOfxImageEffectSimpleSourceClipName);
      return kOfxStatOK;
    }
  }

  // In this case do the default, which in this case is to render
  return kOfxStatReplyDefault;
}

////////////////////////////////////////////////////////////////////////////////
// rendering routines
template <class T> inline T 
Clamp(T v, int min, int max)
{
  if(v < T(min)) return T(min);
  if(v > T(max)) return T(max);
  return v;
}

static inline int
Lerp(int a, int b, float v)
{
  return int(a + (b - a) * v);
}

static inline float
Lerp(float a, float b, float v)
{
  return float(a + (b - a) * v);
}

// look up a pixel in the image, does bounds checking to see if it is in the image rectangle
template <class PIX> inline PIX *
pixelAddress(PIX *img, OfxRectI rect, int x, int y, int bytesPerLine)
{  
  if(x < rect.x1 || x >= rect.x2 || y < rect.y1 || y > rect.y2)
    return 0;
  PIX *pix = (PIX *) (((char *) img) + (y - rect.y1) * bytesPerLine);
  pix += x - rect.x1;  
  return pix;
}

////////////////////////////////////////////////////////////////////////////////
// base class to process images with
class Processor {
protected :
  OfxImageEffectHandle effect;
  OfxRGBAColourD colour;
  OfxRectI       position;
  bool           unpremultiplied;
  void *srcV, *dstV; 
  OfxRectI srcRect, dstRect;
  OfxRectI  window;
  int srcBytesPerLine, dstBytesPerLine;

public :
  Processor(OfxImageEffectHandle eff,
	    OfxRectI pos,
	    OfxRGBAColourD col,
	    bool unpremult,
            void *src, OfxRectI sRect, int sBytesPerLine,
            void *dst, OfxRectI dRect, int dBytesPerLine,
            OfxRectI  win)
    : effect(eff)
    , colour(col)
    , position(pos)
    , unpremultiplied(unpremult)
    , srcV(src)
    , dstV(dst)
    , srcRect(sRect)
    , dstRect(dRect)
    , window(win)
    , srcBytesPerLine(sBytesPerLine)
    , dstBytesPerLine(dBytesPerLine)
  {}

  static void multiThreadProcessing(unsigned int threadId, unsigned int nThreads, void *arg);
  virtual void doProcessing(OfxRectI window) = 0;
  void process(void);
};

// function call once for each thread by the host
void
Processor::multiThreadProcessing(unsigned int threadId, unsigned int nThreads, void *arg)
{	
  Processor *proc = (Processor *) arg;

  // slice the y range into the number of threads it has
  unsigned int dy = proc->window.y2 - proc->window.y1;
  
  unsigned int y1 = proc->window.y1 + threadId * dy/nThreads;
  unsigned int y2 = proc->window.y1 + Minimum((threadId + 1) * dy/nThreads, dy);

  OfxRectI win = proc->window;
  win.y1 = y1; win.y2 = y2;

  // and render that thread on each
  proc->doProcessing(win);  
}

// function to kick off rendering across multiple CPUs
void
Processor::process(void)
{
  unsigned int nThreads;
  gThreadHost->multiThreadNumCPUs(&nThreads);
  gThreadHost->multiThread(multiThreadProcessing, nThreads, (void *) this);
}

// template to do the RGBA processing
template <class PIX, int max, int isFloat>
class ProcessRGBA : public Processor{
public :
  ProcessRGBA(OfxImageEffectHandle eff,
              OfxRectI pos,
              OfxRGBAColourD col,
              bool unpremult,
              void *src, OfxRectI sRect, int sBytesPerLine,
              void *dst, OfxRectI dRect, int dBytesPerLine,
              OfxRectI  win)
    : Processor(eff,
                pos, col, unpremult,
                src,  sRect,  sBytesPerLine,
                dst,  dRect,  dBytesPerLine,
                win)
  {
  }

  void doProcessing(OfxRectI procWindow)
  {
    PIX *src = (PIX *) srcV;
    PIX *dst = (PIX *) dstV;

    // black pixel for the default back ground
    PIX black;
    black.r = black.g = black.b = black.a = 0;

    // value and premulitplied of the colour scaled up to quantisation space
    PIX value, premultValue;
    if(isFloat) {
      // no need to clamp
      value.r = colour.r * max;
      value.g = colour.g * max;
      value.b = colour.b * max;
      value.a = colour.a * max;

      premultValue.r = colour.r * max * colour.a;
      premultValue.g = colour.g * max * colour.a;
      premultValue.b = colour.b * max * colour.a;
      premultValue.a = colour.a * max * colour.a;
    }
    else {
      // we have to clamp
      value.r = Clamp(colour.r * max, 0, max);
      value.g = Clamp(colour.g * max, 0, max);
      value.b = Clamp(colour.b * max, 0, max);
      value.a = Clamp(colour.a * max, 0, max);

      premultValue.r = Clamp(colour.r * max * colour.a, 0, max);
      premultValue.g = Clamp(colour.g * max * colour.a, 0, max);
      premultValue.b = Clamp(colour.b * max * colour.a, 0, max);
      premultValue.a = Clamp(colour.a * max * colour.a, 0, max);
    }

    for(int y = procWindow.y1; y < procWindow.y2; y++) {
      if(gEffectHost->abort(effect)) break;

      PIX *dstPix = pixelAddress(dst, dstRect, procWindow.x1, y, dstBytesPerLine);

      for(int x = procWindow.x1; x < procWindow.x2; x++) {
        
        // if a generator, we have no source
        PIX *srcPix = 0;
        if(src)
          srcPix = pixelAddress(src, srcRect, x, y, srcBytesPerLine);        
        
        if(x < position.x1 || x >= position.x2 || y < position.y1 || y >= position.y2) {
          // we are outside the rectangle
          *dstPix = srcPix ? *srcPix : black;
          if(srcPix)
            srcPix++;
        }
        else {
          // we are inside the rectangle, composite it over the source image
          if(srcPix) {
            if(unpremultiplied) {	
              // we have to premultiply, then unpremultiply the composite
              float a = srcPix->a + value.a - (srcPix->a * value.a)/max;
              float r, g, b;
              if(srcPix->a == 0) {
                r = g = b = 0;
              }
              else {
                r = Lerp(srcPix->r * max/srcPix->a, value.r, colour.a) * a/max;
                g = Lerp(srcPix->g * max/srcPix->a, value.g, colour.a) * a/max;
                b = Lerp(srcPix->b * max/srcPix->a, value.b, colour.a) * a/max;
              }

              // clamp or not depending on if it is floating
              if(isFloat) {
                dstPix->r = r;
                dstPix->g = g;
                dstPix->b = b;
                dstPix->a = a;
              }
              else {
                dstPix->r = Clamp(r, 0, max);
                dstPix->g = Clamp(g, 0, max);
                dstPix->b = Clamp(b, 0, max);
                dstPix->a = Clamp(a, 0, max);
              }
            }
            else {
              // source is premultiplied, easier composite
              if(isFloat) {
                dstPix->r = Lerp(srcPix->r, value.r, colour.a);
                dstPix->g = Lerp(srcPix->g, value.g, colour.a);
                dstPix->b = Lerp(srcPix->b, value.b, colour.a);
                dstPix->a = srcPix->a + value.a - (srcPix->a * value.a)/max;
              }
              else {
                dstPix->r = Clamp(int(Lerp(srcPix->r, value.r, colour.a)), 0, max);
                dstPix->g = Clamp(int(Lerp(srcPix->g, value.g, colour.a)), 0, max);
                dstPix->b = Clamp(int(Lerp(srcPix->b, value.b, colour.a)), 0, max);
                dstPix->a = Clamp(int(srcPix->a + value.a - (srcPix->a * value.a)/max), 0, max);
              }
            }

            srcPix++;
          }
          else {
            // no src pixel, just set it
            if(unpremultiplied)
              *dstPix = premultValue;
            else
              *dstPix = value;
          }
        }
        dstPix++;
      }
    }
  }
};

// template to do the Alpha processing
template <class PIX, int max, int isFloat>
class ProcessAlpha : public Processor {
 public :
  ProcessAlpha(OfxImageEffectHandle inst,
	       OfxRectI pos,
	       OfxRGBAColourD col,
	       void *src, OfxRectI sRect, int sBytesPerLine,
	       void *dst, OfxRectI dRect, int dBytesPerLine,
	       OfxRectI  win)
    : Processor(inst,
		pos, col, 1,
                src,  sRect,  sBytesPerLine,
                dst,  dRect,  dBytesPerLine,
                win)
  {
  }

  void doProcessing(OfxRectI procWindow)
  {
    PIX *src = (PIX *) srcV;
    PIX *dst = (PIX *) dstV;

    PIX value;
    if(isFloat) {
      value = colour.a * max;
    }
    else {
      value = Clamp(colour.a * max, 0, max);
    }
    
    for(int y = procWindow.y1; y < procWindow.y2; y++) {
      if(gEffectHost->abort(effect)) break;

      PIX *dstPix = pixelAddress(dst, dstRect, procWindow.x1, y, dstBytesPerLine);

      for(int x = procWindow.x1; x < procWindow.x2; x++) {
        
        // if a generator, we have no source
        PIX *srcPix = 0;
        if(src)
          srcPix = pixelAddress(src, srcRect, x, y, srcBytesPerLine);        
        
        if(x < position.x1 || x >= position.x2 || y < position.y1 || y >= position.y2) {
          // we are outside the rectangle we are drawing?
          *dstPix = srcPix ? *srcPix : 0;
          if(srcPix)
            srcPix++;
        }
        else {
          // we are inside the rectangle, set it to the alpha of the colour
          if(srcPix) {
            // switch will be compiled out
            if(isFloat) {
              *dstPix = *srcPix + value - *srcPix * value;
            }
            else {
              *dstPix = Clamp(int(*srcPix + value - *srcPix * value), 0, max);
            }
            srcPix++;
          }
          else {
            *dstPix = value;
          }
        }
        dstPix++;
      }
    }
  }
};

// the process code  that the host sees
static OfxStatus render(OfxImageEffectHandle effect,
                        OfxPropertySetHandle inArgs,
                        OfxPropertySetHandle /*outArgs*/)
{
  // get the render window and the time from the inArgs
  OfxTime time;
  OfxRectI renderWindow;
  OfxStatus status = kOfxStatOK;
  
  gPropHost->propGetDouble(inArgs, kOfxPropTime, 0, &time);
  gPropHost->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4, &renderWindow.x1);

  // retrieve any instance data associated with this effect
  MyInstanceData *myData = getMyInstanceData(effect);

  // property handles and members of each image
  // in reality, we would put this in a struct as the C++ support layer does
  OfxPropertySetHandle sourceImg = NULL, outputImg = NULL;
  int srcRowBytes = 0, srcBitDepth, dstRowBytes, dstBitDepth;
  bool srcIsAlpha, dstIsAlpha;
  OfxRectI dstRect, srcRect = {0};
  void *src = NULL, *dst;
  
  // by default we are premultiplied
  bool unpremultiplied = false;
  
  try {
    outputImg = ofxuGetImage(myData->outputClip, time, dstRowBytes, dstBitDepth, dstIsAlpha, dstRect, dst);
    if(outputImg == NULL) throw OfxuNoImageException();


    if(myData->context != eIsGenerator) {
      sourceImg = ofxuGetImage(myData->sourceClip, time, srcRowBytes, srcBitDepth, srcIsAlpha, srcRect, src);
      if(sourceImg == NULL) throw OfxuNoImageException();
      unpremultiplied = ofxuIsUnPremultiplied(myData->sourceClip);
    }
    
    // get the render scale
    OfxPointD renderScale;
    gPropHost->propGetDoubleN(inArgs, kOfxImageEffectPropRenderScale, 2, &renderScale.x);

    // If we are rendering a single field, then have a field scale of 0.5
    double fieldScale = 1.0;
    char *field;
    gPropHost->propGetString(outputImg, kOfxImagePropField, 0, &field);
    if(strcmp(field, kOfxImageFieldLower) == 0 || strcmp(field, kOfxImageFieldUpper) == 0)
      fieldScale = 0.5;

    // get the pixel aspect ratio from the image
    double pixelAspectRatio;
    gPropHost->propGetDouble(outputImg, kOfxImagePropPixelAspectRatio, 0, &pixelAspectRatio);

    // get the rect in canonical coordinates  
    OfxRectD rect;
    getCannonicalRect(effect, time, rect);
  
    // Turn that into pixel coordinates
    OfxRectI rectI;
    rectI.x1 = int(rect.x1 * renderScale.x / pixelAspectRatio);
    rectI.x2 = int(rect.x2 * renderScale.x / pixelAspectRatio);
    rectI.y1 = int(rect.y1 * renderScale.y * fieldScale);
    rectI.y2 = int(rect.y2 * renderScale.y * fieldScale);

    // get the colour of it
    OfxRGBAColourD colour;
    gParamHost->paramGetValueAtTime(myData->colourParam, time, &colour.r, &colour.g, &colour.b, &colour.a);

    // do the rendering
    if(!dstIsAlpha) {
      switch(dstBitDepth) {
      case 8 : {      
        ProcessRGBA<OfxRGBAColourB, 255, 0> fred(effect, rectI, colour, unpremultiplied,
                                                 src, srcRect, srcRowBytes,
                                                 dst, dstRect, dstRowBytes,
                                                 renderWindow);
        fred.process();                                          
      }
        break;

      case 16 : {
        ProcessRGBA<OfxRGBAColourS, 65535, 0> fred(effect, rectI, colour, unpremultiplied,
                                                   src, srcRect, srcRowBytes,
                                                   dst, dstRect, dstRowBytes,
                                                   renderWindow);
        fred.process();           
      }                          
        break;

      case 32 : {
        ProcessRGBA<OfxRGBAColourF, 1, 1> fred(effect, rectI, colour, unpremultiplied,
                                               src, srcRect, srcRowBytes,
                                               dst, dstRect, dstRowBytes,
                                               renderWindow);
        fred.process();                                          
        break;
      }
      }
    }
    else {
      switch(dstBitDepth) {
      case 8 : {
        ProcessAlpha<unsigned char, 255, 0> fred(effect, rectI, colour, 
                                                 src, srcRect, srcRowBytes,
                                                 dst, dstRect, dstRowBytes,
                                                 renderWindow);
        fred.process();                                                                                  
      }
        break;

      case 16 : {
        ProcessAlpha<unsigned short, 65535, 0> fred(effect, rectI, colour, 
                                                    src, srcRect, srcRowBytes,
                                                    dst, dstRect, dstRowBytes,
                                                    renderWindow);
        fred.process();           
      }                          
        break;

      case 32 : {
        ProcessAlpha<float, 1, 1> fred(effect, rectI, colour,
                                       src, srcRect, srcRowBytes,
                                       dst, dstRect, dstRowBytes,
                                       renderWindow);
        fred.process();           
      }                          
        break;
      }
    }
  }
  catch(OfxuNoImageException &ex) {
    // if we were interrupted, the failed fetch is fine, just return kOfxStatOK
    // otherwise, something weird happened
    if(!gEffectHost->abort(effect)) {
      status = kOfxStatFailed;
    }
  }

  // release the data pointers
  if(sourceImg)
    gEffectHost->clipReleaseImage(sourceImg);
  if(outputImg)
    gEffectHost->clipReleaseImage(outputImg);
  
  return status;
}

// Set our clip preferences 
static OfxStatus 
getClipPreferences(OfxImageEffectHandle effect, OfxPropertySetHandle /*inArgs*/, OfxPropertySetHandle outArgs)
{
  // retrieve any instance data associated with this effect
  MyInstanceData *myData = getMyInstanceData(effect);
  
  if(myData->context == eIsGenerator) {
    // as a generator we create premultiplied output
    gPropHost->propSetString(outArgs, kOfxImageEffectPropPreMultiplication, 0, kOfxImagePreMultiplied);
  }
  else {
    OfxPropertySetHandle clipProps;
    gEffectHost->clipGetPropertySet(myData->sourceClip, &clipProps);

    //  premultiplication is the same as the source
    char *premult;
    gPropHost->propGetString(clipProps, kOfxImageEffectPropPreMultiplication, 0, &premult);

    gPropHost->propSetString(outArgs, kOfxImageEffectPropPreMultiplication, 0, premult);
  }

  return kOfxStatOK;
}

//  describe the plugin in context
static OfxStatus
describeInContext(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs)
{
  // get the context from the inArgs handle
  char *context;
  gPropHost->propGetString(inArgs, kOfxImageEffectPropContext, 0, &context);
  bool isGeneratorContext = strcmp(context, kOfxImageEffectContextGenerator) == 0;

  OfxPropertySetHandle clipProps;
  // define the single output clip in both contexts
  gEffectHost->clipDefine(effect, kOfxImageEffectOutputClipName, &clipProps);

  // set the component types we can handle on out output
  gPropHost->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
  gPropHost->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentAlpha);
  gPropHost->propSetString(clipProps, kOfxImageClipPropFieldExtraction, 0, kOfxImageFieldSingle);

  if(!isGeneratorContext) {
    // define the single source clip in filter and general contexts
    gEffectHost->clipDefine(effect, kOfxImageEffectSimpleSourceClipName, &clipProps);

    // set the component types we can handle on our main input
    gPropHost->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gPropHost->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentAlpha);
    gPropHost->propSetString(clipProps, kOfxImageClipPropFieldExtraction, 0, kOfxImageFieldSingle);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // define the parameters for this context
  
  // get a pointer to the effect's parameter set
  OfxParamSetHandle paramSet;
  gEffectHost->getParamSet(effect, &paramSet);

  // our 2 corners are normalised spatial 2D doubles
  OfxPropertySetHandle paramProps;

  gParamHost->paramDefine(paramSet, kOfxParamTypeDouble2D, "corner1", &paramProps);
  gPropHost->propSetString(paramProps, kOfxParamPropDoubleType, 0, kOfxParamDoubleTypeXYAbsolute);
  gPropHost->propSetString(paramProps, kOfxParamPropDefaultCoordinateSystem, 0, kOfxParamCoordinatesNormalised);
  gPropHost->propSetDouble(paramProps, kOfxParamPropDefault, 0, 0.4);
  gPropHost->propSetDouble(paramProps, kOfxParamPropDefault, 1, 0.4);
  gPropHost->propSetString(paramProps, kOfxParamPropHint, 0, "A corner of the rectangle to draw");
  gPropHost->propSetString(paramProps, kOfxPropLabel, 0, "Corner 1");
  
  gParamHost->paramDefine(paramSet, kOfxParamTypeDouble2D, "corner2", &paramProps);
  gPropHost->propSetString(paramProps, kOfxParamPropDoubleType, 0, kOfxParamDoubleTypeXYAbsolute);
  gPropHost->propSetString(paramProps, kOfxParamPropDefaultCoordinateSystem, 0, kOfxParamCoordinatesNormalised);
  gPropHost->propSetDouble(paramProps, kOfxParamPropDefault, 0, 0.6);
  gPropHost->propSetDouble(paramProps, kOfxParamPropDefault, 1, 0.6);
  gPropHost->propSetString(paramProps, kOfxParamPropHint, 0, "A corner of the rectangle to draw");
  gPropHost->propSetString(paramProps, kOfxPropLabel, 0, "Corner 2");
  gPropHost->propSetDouble(paramProps, kOfxParamPropDefault, 0, 0);
  gPropHost->propSetDouble(paramProps, kOfxParamPropDefault, 1, 0);
  gPropHost->propSetDouble(paramProps, kOfxParamPropDefault, 2, 0);
  gPropHost->propSetDouble(paramProps, kOfxParamPropDefault, 3, 1);

  // make an rgba colour parameter
  gParamHost->paramDefine(paramSet, kOfxParamTypeRGBA, "colour", &paramProps);
  gPropHost->propSetString(paramProps, kOfxParamPropHint, 0, "The colour of the rectangle");
  gPropHost->propSetString(paramProps, kOfxParamPropScriptName, 0, "colour");
  gPropHost->propSetString(paramProps, kOfxPropLabel, 0, "Colour");


  return kOfxStatOK;
}

////////////////////////////////////////////////////////////////////////////////
// the plugin's description routine
static OfxStatus
describe(OfxImageEffectHandle effect)
{
  // first fetch the host APIs, this cannot be done before this call
  OfxStatus stat;
  if((stat = ofxuFetchHostSuites()) != kOfxStatOK)
    return stat;
  
  // get a pointer to the effect's set of properties
  OfxPropertySetHandle effectProps;
  gEffectHost->getPropertySet(effect, &effectProps);


  // We can render both fields in a fielded image in one hit if there is no animation
  // So set the flag that allows us to do this
  gPropHost->propSetInt(effectProps, kOfxImageEffectPluginPropFieldRenderTwiceAlways, 0, 0);

  // say we cannot support multiple pixel depths on in and out
  gPropHost->propSetInt(effectProps, kOfxImageEffectPropSupportsMultipleClipDepths, 0, 0);
  
  // set the bit depths the plugin can handle
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthByte);
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthShort);
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 2, kOfxBitDepthFloat);

  // set some labels and the group it belongs to
  gPropHost->propSetString(effectProps, kOfxPropLabel, 0, "OFX Rectangle");
  gPropHost->propSetString(effectProps, kOfxImageEffectPluginPropGrouping, 0, "OFX Example");

  // define the contexts we can be used in
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextGenerator);
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 1, kOfxImageEffectContextFilter);
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 2, kOfxImageEffectContextGeneral);
  
  return kOfxStatOK;
}

////////////////////////////////////////////////////////////////////////////////
// The main function
static OfxStatus
pluginMain(const char *action,  const void *handle, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs)
{
  try {
  // cast to appropriate type
  OfxImageEffectHandle effect = (OfxImageEffectHandle ) handle;

  if(strcmp(action, kOfxActionDescribe) == 0) {
    return describe(effect);
  }
  else if(strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
    return describeInContext(effect, inArgs);
  }
  else if(strcmp(action, kOfxActionCreateInstance) == 0) {
    return createInstance(effect);
  } 
  else if(strcmp(action, kOfxActionDestroyInstance) == 0) {
    return destroyInstance(effect);
  } 
  else if(strcmp(action, kOfxImageEffectActionIsIdentity) == 0) {
    return isIdentity(effect, inArgs, outArgs);
  }    
  else if(strcmp(action, kOfxImageEffectActionRender) == 0) {
    return render(effect, inArgs, outArgs);
  }    
  else if(strcmp(action, kOfxImageEffectActionGetRegionOfDefinition) == 0) {
    return getSpatialRoD(effect, inArgs, outArgs);
  }  
  else if(strcmp(action, kOfxImageEffectActionGetRegionsOfInterest) == 0) {
    return getSpatialRoI(effect, inArgs, outArgs);
  }  
  else if(strcmp(action, kOfxImageEffectActionGetClipPreferences) == 0) {
    return getClipPreferences(effect, inArgs, outArgs);
  }  
  } catch (std::bad_alloc) {
    // catch memory
    //std::cout << "OFX Plugin Memory error." << std::endl;
    return kOfxStatErrMemory;
  } catch ( const std::exception& e ) {
    // standard exceptions
    //std::cout << "OFX Plugin error: " << e.what() << std::endl;
    return kOfxStatErrUnknown;
  } catch (int err) {
    // ho hum, gone wrong somehow
    return err;
  } catch ( ... ) {
    // everything else
    //std::cout << "OFX Plugin error" << std::endl;
    return kOfxStatErrUnknown;
  }
    
  // other actions to take the default value
  return kOfxStatReplyDefault;
}

// function to set the host structure
static void
setHostFunc(OfxHost *hostStruct)
{
  gHost         = hostStruct;
}

////////////////////////////////////////////////////////////////////////////////
// the plugin struct 
static OfxPlugin basicPlugin = 
{       
  kOfxImageEffectPluginApi,
  1,
  "uk.co.thefoundry.GeneratorExample",
  1,
  0,
  setHostFunc,
  pluginMain
};
   
// the two mandated functions
EXPORT OfxPlugin *
OfxGetPlugin(int nth)
{
  if(nth == 0)
    return &basicPlugin;
  return 0;
}
 
EXPORT int
OfxGetNumberOfPlugins(void)
{       
  return 1;
}
