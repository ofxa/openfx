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
  Ofx Example plugin that shows how a plugin can manipulate pixel depths.

  It is meant to illustrate certain features of the API, as opposed to being a perfectly
  crafted piece of image processing software.

  The main features are
    - how to map pixel depths
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

// Message id for the message posted when we don't have enough bits
#define kMessageNotEnoughBits "MessageIDNotEnoughBits"

template <class T> inline T Maximum(T a, T b) {return a > b ? a : b;}
template <class T> inline T Minimum(T a, T b) {return a < b ? a : b;}


static bool gSupportsBytes  = false;
static bool gSupportsShorts = false;
static bool gSupportsFloats = false;
static int gDepthParamToBytes[3]; // maps the value of the bit depth param to a host supported bit depth

// pointers64 to various bits of the host
OfxHost               *gHost;
OfxImageEffectSuiteV1 *gEffectHost = 0;
OfxPropertySuiteV1    *gPropHost = 0;
OfxParameterSuiteV1   *gParamHost = 0;
OfxMemorySuiteV1      *gMemoryHost = 0;
OfxMultiThreadSuiteV1 *gThreadHost = 0;
OfxMessageSuiteV1     *gMessageSuite = 0;
OfxInteractSuiteV1    *gInteractHost = 0;

// private instance data type
struct MyInstanceData {
  // handles to the clips we deal with
  OfxImageClipHandle sourceClip;
  OfxImageClipHandle outputClip;

  // handles to a our parameters
  OfxParamHandle depthParam;
};


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

  MyInstanceData *myData = new MyInstanceData;

  // cache away param handles
  gParamHost->paramGetHandle(paramSet, "depth", &myData->depthParam, 0);

  // cache away clip handles
  gEffectHost->clipGetHandle(effect, "Source", &myData->sourceClip, 0);
  gEffectHost->clipGetHandle(effect, "Output", &myData->outputClip, 0);
  
  ofxuSetEffectInstanceData(effect, (void *) myData);

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

// are the settings of the effect performing an identity operation
static OfxStatus
isIdentity(OfxImageEffectHandle  effect,
	   OfxPropertySetHandle /*inArgs*/,
	   OfxPropertySetHandle outArgs)
{
  // retrieve any instance data associated with this effect
  MyInstanceData *myData = getMyInstanceData(effect);
  
  // get the render window and the time from the inArgs
  //OfxTime time;
  //OfxRectI renderWindow;
  
  // get the src depth
  int srcDepth = ofxuGetClipPixelDepth(myData->sourceClip);

  // get the dst depth
  int dstDepth = ofxuGetClipPixelDepth(myData->outputClip);

  // if the depths are the same we have no work to do!
  if(srcDepth == dstDepth) {
    // set the property in the out args indicating which is the identity clip
    gPropHost->propSetString(outArgs, kOfxPropName, 0, "Source");
    return kOfxStatOK;
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
  OfxImageEffectHandle instance;
  int nComponents;
  void *srcV, *dstV; 
  OfxRectI srcRect, dstRect;
  int srcBytesPerLine, dstBytesPerLine;
  OfxRectI  window;

public :
  Processor(const Processor &p)
    : instance(p.instance)
    , nComponents(p.nComponents)
    , srcV(p.srcV)
    , dstV(p.dstV)
    , srcRect(p.srcRect)
    , dstRect(p.dstRect)
    , srcBytesPerLine(p.srcBytesPerLine)
    , dstBytesPerLine(p.dstBytesPerLine)
    , window(p.window)
  {}  

  Processor(OfxImageEffectHandle inst, int nComps,
            void *src, OfxRectI sRect, int sBytesPerLine,
            void *dst, OfxRectI dRect, int dBytesPerLine,
            OfxRectI  win)
    : instance(inst)
    , nComponents(nComps)
    , srcV(src)
    , dstV(dst)
    , srcRect(sRect)
    , dstRect(dRect)
    , srcBytesPerLine(sBytesPerLine)
    , dstBytesPerLine(dBytesPerLine)
    , window(win)
  {}  

  static void multiThreadProcessing(unsigned int threadId, unsigned int nThreads, void *arg);
  virtual void doProcessing(OfxRectI window);
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

void 
Processor::doProcessing(OfxRectI /*window*/)
{
}

// template to do the RGBA processing
template <class SRCPIX, int kSrcMax, int srcIsFloat,
	  class DSTPIX, int kDstMax, int dstIsFloat>
class ProcessPix : public Processor {
 public :
  ProcessPix(const Processor &p)
    : Processor(p)
  {
    process();
  }

  void doProcessing(OfxRectI procWindow)
  {
    SRCPIX *src = (SRCPIX *) srcV;
    DSTPIX *dst = (DSTPIX *) dstV;

    float scaleF;
    scaleF = float(kDstMax)/float(kSrcMax);

    for(int y = procWindow.y1; y < procWindow.y2; y++) {
      if(gEffectHost->abort(instance)) break;

      DSTPIX *dstPix = pixelAddress(dst, dstRect, procWindow.x1, y, dstBytesPerLine);

      for(int x = procWindow.x1; x < procWindow.x2; x++) {
        
        // if a generator, we have no source
        SRCPIX *srcPix = pixelAddress(src, srcRect, x, y, srcBytesPerLine);        
        
        // change my pixel depths
        if(srcPix) {
          for(int c = 0; c < nComponents; c++) {
            if(dstIsFloat)
              dstPix[c] = srcPix[c] * scaleF;
            else if (srcIsFloat) 
              dstPix[c] = Clamp(srcPix[c] * scaleF, 0, kDstMax);
            else
              dstPix[c] = Clamp(srcPix[c] * kDstMax/kSrcMax, 0, kDstMax);	    
          }
          srcPix += nComponents;
        }
        else {
          for(int c = 0; c < nComponents; c++)
            dstPix[c] = 0;
        }
        dstPix += nComponents;
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
  int srcRowBytes, srcBitDepth, dstRowBytes, dstBitDepth;
  bool srcIsAlpha, dstIsAlpha;
  OfxRectI dstRect, srcRect;
  void *src, *dst;

  try {
    outputImg = ofxuGetImage(myData->outputClip, time, dstRowBytes, dstBitDepth, dstIsAlpha, dstRect, dst);
    if(outputImg == NULL) throw OfxuNoImageException();

    sourceImg = ofxuGetImage(myData->sourceClip, time, srcRowBytes, srcBitDepth, srcIsAlpha, srcRect, src);
    if(sourceImg == NULL) throw OfxuNoImageException();
    
    int nComponents = dstIsAlpha ? 1 : 4;
    
    // set up the processor that we pass to the individual constructors
    Processor proc(effect, nComponents,
                   src, srcRect, srcRowBytes,
                   dst, dstRect, dstRowBytes,
                   renderWindow);
    
    // now instantiate the templated processor depending on src and dest pixel types, 9 cases in all
    switch(dstBitDepth) {
    case 8 : {
      switch(srcBitDepth) {
      case 8 :  {ProcessPix<unsigned char,  255,   0, unsigned char, 255, 0> pixProc(proc); break;}
      case 16 : {ProcessPix<unsigned short, 65535, 0, unsigned char, 255, 0> pixProc(proc); break;}
      case 32 : {ProcessPix<float,          1,     1, unsigned char, 255, 0> pixProc(proc); break;}
      }
    }
      break;

    case 16 : {
      switch(srcBitDepth) {
      case 8 :  {ProcessPix<unsigned char,  255,   0, unsigned short, 65535, 0> pixProc(proc); break;}
      case 16 : {ProcessPix<unsigned short, 65535, 0, unsigned short, 65535, 0> pixProc(proc); break;}
      case 32 : {ProcessPix<float,          1,     1, unsigned short, 65535, 0> pixProc(proc); break;}
      }
    }
      break;

    case 32 : {
      switch(srcBitDepth) {
      case 8 :  {ProcessPix<unsigned char,  255,   0, float, 1, 1> pixProc(proc); break;}
      case 16 : {ProcessPix<unsigned short, 65535, 0, float, 1, 1> pixProc(proc); break;}
      case 32 : {ProcessPix<float,          1,     1, float, 1, 1> pixProc(proc); break;}
      }
    }                          
      break;
    }
  }
  catch(OfxuNoImageException &ex) {
    // if we were interrupted, the failed fetch is fine, just return kOfxStatOK
    // otherwise, something weird happened
    if(!gEffectHost->abort(effect)) {
      status = kOfxStatFailed;
    }
  }

  // release the data pointers;
  if(sourceImg)
    gEffectHost->clipReleaseImage(sourceImg);
  if(outputImg)
    gEffectHost->clipReleaseImage(outputImg);
  
  return status;
}

// Set our clip preferences 
static OfxStatus 
getClipPreferences(OfxImageEffectHandle effect,
		   OfxPropertySetHandle /*inArgs*/,
		   OfxPropertySetHandle outArgs)
{
  // retrieve any instance data associated with this effect
  MyInstanceData *myData = getMyInstanceData(effect);
  
  // fetch the depth parameter value
  int depthVal;
  gParamHost->paramGetValue(myData->depthParam, &depthVal);

  // and set the output depths based on that
  switch(gDepthParamToBytes[depthVal]) {
  // byte
  case 8 : gPropHost->propSetString(outArgs, "OfxImageClipPropDepth_Output", 0, kOfxBitDepthByte); break;
  // short
  case 16 : gPropHost->propSetString(outArgs, "OfxImageClipPropDepth_Output", 0, kOfxBitDepthShort); break;
  // float
  case 32 : gPropHost->propSetString(outArgs, "OfxImageClipPropDepth_Output", 0, kOfxBitDepthFloat); break;
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
  //bool isGeneratorContext = strcmp(context, kOfxImageEffectContextGenerator) == 0;

  OfxPropertySetHandle clipProps;
  // define the single output clip in both contexts
  gEffectHost->clipDefine(effect, "Output", &clipProps);

  // set the component types we can handle on out output
  gPropHost->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
  gPropHost->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentAlpha);
  gPropHost->propSetString(clipProps, kOfxImageClipPropFieldExtraction, 0, kOfxImageFieldSingle);

  // define the single source clip in filter and general contexts
  gEffectHost->clipDefine(effect, "Source", &clipProps);

  // set the component types we can handle on our main input
  gPropHost->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
  gPropHost->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentAlpha);
  gPropHost->propSetString(clipProps, kOfxImageClipPropFieldExtraction, 0, kOfxImageFieldSingle);

  ////////////////////////////////////////////////////////////////////////////////
  // define the parameters for this context
  OfxParamSetHandle paramSet;
  gEffectHost->getParamSet(effect, &paramSet);

  //OfxParamHandle param;
  OfxPropertySetHandle paramProps;

  // single choice parameter
  gParamHost->paramDefine(paramSet, kOfxParamTypeChoice, "depth", &paramProps);
  gPropHost->propSetString(paramProps, kOfxParamPropHint, 0, "What pixel depth to convert the image to");
  gPropHost->propSetString(paramProps, kOfxPropLabel, 0, "Depth");

  // set the options, depending on the bit depths the host supports
  int i = 0;
  if(gSupportsBytes)  gPropHost->propSetString(paramProps, kOfxParamPropChoiceOption, i++, "Byte");
  if(gSupportsShorts) gPropHost->propSetString(paramProps, kOfxParamPropChoiceOption, i++, "Short");
  if(gSupportsFloats) gPropHost->propSetString(paramProps, kOfxParamPropChoiceOption, i++, "Float");

  // we convert things to 8 bits by default
  gPropHost->propSetInt(paramProps, kOfxParamPropDefault, 0, 0);

  // say that the pixel depth affects the clip preferences
  OfxPropertySetHandle effectProps;
  gEffectHost->getPropertySet(effect, &effectProps);
  gPropHost->propSetString(effectProps, kOfxImageEffectPropClipPreferencesSlaveParam, 0, "depth");

  return kOfxStatOK;
}

////////////////////////////////////////////////////////////////////////////////
// the plugin's description routine
static OfxStatus
describe(OfxImageEffectHandle  effect)
{
  // first fetch the host APIs, this cannot be done before this call
  OfxStatus stat;
  if((stat = ofxuFetchHostSuites()) != kOfxStatOK)
    return stat;

  int hostSupportsMultipleDepths;
  // record a few host features
  gPropHost->propGetInt(gHost->host, kOfxImageEffectPropSupportsMultipleClipDepths, 0, &hostSupportsMultipleDepths);

  // see how many bit depths the host supports, this affects our parameter values
  int nHostDepths;
  gPropHost->propGetDimension(gHost->host, kOfxImageEffectPropSupportedPixelDepths, &nHostDepths);

  // If the host cannot support multiple bit depths on in and out clips or it only supports 1 bit depth
  // we can't do any work, so refuse to load and explain why.
  if(!hostSupportsMultipleDepths || nHostDepths == 1) {
    // post a message
    // - disabled, because posting a message within describe() crashes Nuke 6
    //gMessageSuite->message(effect, kOfxMessageError, kMessageNotEnoughBits,
	//		   "OFX GeneratorExample : cannot run on this application because the it does not allow effects to change the bit depth of images.");

    // and refuse to load
    // - disabled, because Nuke 6 still loads it - it's better to load it even if it's non-functional
    //return kOfxStatErrMissingHostFeature;
  }

  // get the property handle for the plugin
  OfxPropertySetHandle effectProps;
  gEffectHost->getPropertySet(effect, &effectProps);

  // We can render both fields in a fielded image in one hit as there is no animation and no spatially dependent parameters
  gPropHost->propSetInt(effectProps, kOfxImageEffectPluginPropFieldRenderTwiceAlways, 0, 0);

  // say we can support multiple pixel depths on in and out
  gPropHost->propSetInt(effectProps, kOfxImageEffectPropSupportsMultipleClipDepths, 0, 1);
  
  // set the bit depths the plugin can handle
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthByte);
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthShort);
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 2, kOfxBitDepthFloat);

  // figure which bit depths are supported
  int i;
  for(i = 0; i < nHostDepths; i++) {
    char *depthStr;
    gPropHost->propGetString(gHost->host, kOfxImageEffectPropSupportedPixelDepths, i, &depthStr);
    int nBits = ofxuMapPixelDepth(depthStr);
    switch(nBits) {
    case 8  : gSupportsBytes  = true; break;
    case 16 : gSupportsShorts = true; break;
    case 32 : gSupportsFloats = true; break;
    }
  }

  // now set a mapping from parameter value to bit depth
  i = 0;
  if(gSupportsBytes)  gDepthParamToBytes[i++] = 8; 
  if(gSupportsShorts) gDepthParamToBytes[i++] = 16;
  if(gSupportsFloats) gDepthParamToBytes[i++] = 32; 

  // set some labels and the group it belongs to
  gPropHost->propSetString(effectProps, kOfxPropLabel, 0, "OFX Depth Converter");
  gPropHost->propSetString(effectProps, kOfxImageEffectPluginPropGrouping, 0, "OFX Example");

  // define the contexts we can be used in
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 1, kOfxImageEffectContextGeneral);
  
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
  "uk.co.thefoundry.DepthConverterExample",
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
