/*
Software License :

Copyright (c) 2012, The Open Effects Association Ltd. All rights reserved.

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
   Direct GPU processing using OpenGL
 */
#include <cstdio>
#include <cstring>
#include <cstdarg>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <stdexcept>
#include <new>

#include "ofxImageEffect.h"
#include "ofxMemory.h"
#include "ofxMultiThread.h"
#include "ofxOpenGLRender.h"

#include "../include/ofxUtilities.H" // example support utils

#if defined __APPLE__ || defined linux || defined __FreeBSD__
#  define EXPORT __attribute__((visibility("default")))
#elif defined _WIN32
#  define EXPORT OfxExport
#else
#  error Not building on your operating system quite yet
#endif

// pointers64 to various bits of the host
OfxHost               *gHost;
OfxImageEffectSuiteV1 *gEffectHost = 0;
OfxPropertySuiteV1    *gPropHost = 0;
OfxParameterSuiteV1   *gParamHost = 0;
OfxMemorySuiteV1      *gMemoryHost = 0;
OfxMultiThreadSuiteV1 *gThreadHost = 0;
OfxMessageSuiteV1     *gMessageSuite = 0;
OfxInteractSuiteV1    *gInteractHost = 0;
OfxImageEffectOpenGLRenderSuiteV1 *gOpenGLSuite = 0;

// some flags about the host's behaviour
int gHostSupportsMultipleBitDepths = false;
int gHostSupportsOpenGL = false;

/*
#define CHECK_STATUS(args) check_status_fun args

static void
check_status_fun(int status, int expected, const char *name)
{
  if (status != expected) {
    fprintf(stderr, "OFX error in %s: expected status %d, got %d\n",
	    name, expected, status);
  }
}
*/

#define DPRINT(args) print_dbg args
void print_dbg(const char *fmt, ...)
{
  char msg[1024];
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(msg, 1023, fmt, ap);
  fwrite(msg, sizeof(char), strlen(msg), stderr);
  fflush(stderr);
#ifdef _WIN32
  OutputDebugString(msg);
#endif
  va_end(ap);
}

// private instance data type
struct MyInstanceData {
  bool isGeneralEffect;

  // handles to the clips we deal with
  OfxImageClipHandle sourceClip;
  OfxImageClipHandle outputClip;

  // handles to our parameters
  OfxParamHandle scaleParam;
  OfxParamHandle sourceScaleParam;
};

/* mandatory function to set up the host structures */


// Convenience wrapper to get private data
static MyInstanceData *
getMyInstanceData( OfxImageEffectHandle effect)
{
  // get the property handle for the plugin
  OfxPropertySetHandle effectProps;
  gEffectHost->getPropertySet(effect, &effectProps);

  // get my data pointer out of that
  MyInstanceData *myData = 0;
  gPropHost->propGetPointer(effectProps,  kOfxPropInstanceData, 0,
			    (void **) &myData);
  return myData;
}

/** @brief Called at load */
static OfxStatus
onLoad(void)
{
  return kOfxStatOK;
}

/** @brief Called before unload */
static OfxStatus
onUnLoad(void)
{
  return kOfxStatOK;
}

//  instance construction
static OfxStatus
createInstance( OfxImageEffectHandle effect)
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
  myData->isGeneralEffect = context && (strcmp(context, kOfxImageEffectContextGeneral) == 0);

  // cache away our param handles
  gParamHost->paramGetHandle(paramSet, "scale", &myData->scaleParam, 0);
  gParamHost->paramGetHandle(paramSet, "source_scale", &myData->sourceScaleParam, 0);

  // cache away our clip handles
  gEffectHost->clipGetHandle(effect, kOfxImageEffectSimpleSourceClipName, &myData->sourceClip, 0);
  gEffectHost->clipGetHandle(effect, kOfxImageEffectOutputClipName, &myData->outputClip, 0);

  // set my private instance data
  gPropHost->propSetPointer(effectProps, kOfxPropInstanceData, 0, (void *) myData);

  return kOfxStatOK;
}

// instance destruction
static OfxStatus
destroyInstance( OfxImageEffectHandle  effect)
{
  // get my instance data
  MyInstanceData *myData = getMyInstanceData(effect);

  // and delete it
  if(myData)
    delete myData;
  return kOfxStatOK;
}

// tells the host what region we are capable of filling
OfxStatus
getSpatialRoD( OfxImageEffectHandle  effect,  OfxPropertySetHandle inArgs,  OfxPropertySetHandle outArgs)
{
  // retrieve any instance data associated with this effect
  MyInstanceData *myData = getMyInstanceData(effect);

  OfxTime time;
  gPropHost->propGetDouble(inArgs, kOfxPropTime, 0, &time);

  // my RoD is the same as my input's
  OfxRectD rod;
  gEffectHost->clipGetRegionOfDefinition(myData->sourceClip, time, &rod);

  // set the rod in the out args
  gPropHost->propSetDoubleN(outArgs, kOfxImageEffectPropRegionOfDefinition, 4, &rod.x1);

  return kOfxStatOK;
}

// tells the host how much of the input we need to fill the given window
OfxStatus
getSpatialRoI( OfxImageEffectHandle  effect,  OfxPropertySetHandle inArgs,  OfxPropertySetHandle outArgs)
{
  // get the RoI the effect is interested in from inArgs
  OfxRectD roi;
  gPropHost->propGetDoubleN(inArgs, kOfxImageEffectPropRegionOfInterest, 4, &roi.x1);

  // the input needed is the same as the output, so set that on the source clip
  gPropHost->propSetDoubleN(outArgs, "OfxImageClipPropRoI_Source", 4, &roi.x1);

  // retrieve any instance data associated with this effect
  MyInstanceData *myData = getMyInstanceData(effect);
  (void)myData;

  return kOfxStatOK;
}

// Tells the host how many frames we can fill, only called in the general context.
// This is actually redundant as this is the default behaviour, but for illustrative
// purposes.
OfxStatus
getTemporalDomain( OfxImageEffectHandle  effect,  OfxPropertySetHandle /*inArgs*/,  OfxPropertySetHandle outArgs)
{
  MyInstanceData *myData = getMyInstanceData(effect);

  double sourceRange[2];

  // get the frame range of the source clip
  OfxPropertySetHandle props; gEffectHost->clipGetPropertySet(myData->sourceClip, &props);
  gPropHost->propGetDoubleN(props, kOfxImageEffectPropFrameRange, 2, sourceRange);

  // set it on the out args
  gPropHost->propSetDoubleN(outArgs, kOfxImageEffectPropFrameRange, 2, sourceRange);

  return kOfxStatOK;
}


// Set our clip preferences
static OfxStatus
getClipPreferences( OfxImageEffectHandle  effect,  OfxPropertySetHandle /*inArgs*/,  OfxPropertySetHandle outArgs)
{
  // retrieve any instance data associated with this effect
  MyInstanceData *myData = getMyInstanceData(effect);

  // get the component type and bit depth of our main input
  int  bitDepth;
  bool isRGBA;
  ofxuClipGetFormat(myData->sourceClip, bitDepth, isRGBA, true); // get the unmapped clip component

  // get the strings used to label the various bit depths
  const char *bitDepthStr = bitDepth == 8 ? kOfxBitDepthByte : (bitDepth == 16 ? kOfxBitDepthShort : kOfxBitDepthFloat);
  const char *componentStr = isRGBA ? kOfxImageComponentRGBA : kOfxImageComponentAlpha;

  // set out output to be the same same as the input, component and bitdepth
  gPropHost->propSetString(outArgs, "OfxImageClipPropComponents_Output", 0, componentStr);
  if(gHostSupportsMultipleBitDepths)
    gPropHost->propSetString(outArgs, "OfxImageClipPropDepth_Output", 0, bitDepthStr);

  return kOfxStatOK;
}

// are the settings of the effect performing an identity operation
static OfxStatus
isIdentity( OfxImageEffectHandle  /*effect*/,
	    OfxPropertySetHandle /*inArgs*/,
	    OfxPropertySetHandle /*outArgs*/)
{
  // In this case do the default, which in this case is to render
  return kOfxStatReplyDefault;
}

////////////////////////////////////////////////////////////////////////////////
// function called when the instance has been changed by anything
static OfxStatus
instanceChanged( OfxImageEffectHandle  /*effect*/,
		 OfxPropertySetHandle /*inArgs*/,
		 OfxPropertySetHandle /*outArgs*/)
{
  // don't trap any others
  return kOfxStatReplyDefault;
}

////////////////////////////////////////////////////////////////////////////////
// rendering routines

// Is image handle a GPU texture?
/*
static bool image_is_texture(OfxPropertySetHandle image)
{
  int tmp;
  return (gOpenGLSuite != NULL) &&
    (gPropHost->propGetInt(image, kOfxImageEffectPropOpenGLTextureIndex, 0, &tmp) == kOfxStatOK);
}
*/

// Render to texture: see http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-14-render-to-texture/


// the process code  that the host sees
static OfxStatus render( OfxImageEffectHandle  instance,
                         OfxPropertySetHandle inArgs,
                         OfxPropertySetHandle /*outArgs*/)
{
  // get the render window and the time from the inArgs
  OfxTime time;
  OfxRectI renderWindow;
  OfxStatus status = kOfxStatOK;

  gPropHost->propGetDouble(inArgs, kOfxPropTime, 0, &time);
  gPropHost->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4, &renderWindow.x1);

  // Retrieve instance data associated with this effect
  MyInstanceData *myData = getMyInstanceData(instance);

  // property handles and members of each image
  OfxPropertySetHandle sourceImg = NULL, outputImg = NULL;
  int gl_enabled = 0;
  int source_texture_index = -1, source_texture_target = -1;
  int output_texture_index = -1, output_texture_target = -1;
  char *tmps;

  DPRINT(("render: openGLSuite %s\n", gOpenGLSuite ? "found" : "not found"));
  if (gOpenGLSuite) {
    gPropHost->propGetInt(inArgs, kOfxImageEffectPropOpenGLEnabled, 0, &gl_enabled);
    DPRINT(("render: openGL rendering %s\n", gl_enabled ? "enabled" : "DISABLED"));
  }
  DPRINT(("Render: window = [%d, %d - %d, %d]\n",
	  renderWindow.x1, renderWindow.y1,
	  renderWindow.x2, renderWindow.y2));

  // For this test, we only process in OpenGL mode.
  if (!gl_enabled) {
    return kOfxStatErrImageFormat;
  }

  // get the output image texture
  status = gOpenGLSuite->clipLoadTexture(myData->outputClip, time, NULL, NULL, &outputImg);
  DPRINT(("openGL: clipLoadTexture (output) returns status %d\n", status));
  if (status != kOfxStatOK) {
    return status;
  }
  status = gPropHost->propGetInt(outputImg, kOfxImageEffectPropOpenGLTextureIndex,
				 0, &output_texture_index);
  if (status != kOfxStatOK) {
    return status;
  }
  status = gPropHost->propGetInt(outputImg, kOfxImageEffectPropOpenGLTextureTarget,
				 0, &output_texture_target);
  if (status != kOfxStatOK) {
    return status;
  }
  status = gPropHost->propGetString(outputImg, kOfxImageEffectPropPixelDepth, 0, &tmps);
  if (status != kOfxStatOK) {
    return status;
  }
  DPRINT(("openGL: output texture index %d, target %d, depth %s\n",
	  output_texture_index, output_texture_target, tmps));

  status = gOpenGLSuite->clipLoadTexture(myData->sourceClip, time, NULL, NULL, &sourceImg);
  DPRINT(("openGL: clipLoadTexture (source) returns status %d\n", status));
  if (status != kOfxStatOK) {
    return status;
  }

  status = gPropHost->propGetInt(sourceImg, kOfxImageEffectPropOpenGLTextureIndex,
				 0, &source_texture_index);
  if (status != kOfxStatOK) {
    return status;
  }
  status = gPropHost->propGetInt(sourceImg, kOfxImageEffectPropOpenGLTextureTarget,
				 0, &source_texture_target);
  if (status != kOfxStatOK) {
    return status;
  }
  status = gPropHost->propGetString(sourceImg, kOfxImageEffectPropPixelDepth, 0, &tmps);
  if (status != kOfxStatOK) {
    return status;
  }
  DPRINT(("openGL: source texture index %d, target %d, depth %s\n",
	  source_texture_index, source_texture_target, tmps));
  // XXX: check status for errors

  // get the scale parameter
  double scale = 1;
  double source_scale = 1;
  gParamHost->paramGetValueAtTime(myData->scaleParam, time, &scale);
  gParamHost->paramGetValueAtTime(myData->sourceScaleParam, time, &source_scale);

  float w = (renderWindow.x2 - renderWindow.x1);
  float h = (renderWindow.y2 - renderWindow.y1);

  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glDisable(GL_BLEND);

  // Draw black into dest to start
  glBegin(GL_QUADS);
  glColor4f(0, 0, 0, 1); //Set the colour to opaque black
  glVertex2f(0, 0);
  glVertex2f(0, h);
  glVertex2f(w, h);
  glVertex2f(w, 0);
  glEnd();

  //
  // Copy source texture to output by drawing a big textured quad
  //

  // set up texture (how much of this is needed?)
  glEnable(source_texture_target);
  glBindTexture(source_texture_target, source_texture_index);
  glTexParameteri(source_texture_target, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(source_texture_target, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(source_texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(source_texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  // textures are oriented with Y up (standard orientation)
  float tymin = 0;
  float tymax = 1;

  // now draw the textured quad containing the source
  glBegin(GL_QUADS);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  glBegin (GL_QUADS);
  glTexCoord2f (0, tymin);
  glVertex2f   (0, 0);
  glTexCoord2f (1.0, tymin);
  glVertex2f   (w * source_scale, 0);
  glTexCoord2f (1.0, tymax);
  glVertex2f   (w * source_scale, h * source_scale);
  glTexCoord2f (0, tymax);
  glVertex2f   (0, h * source_scale);
  glEnd ();

  glDisable(source_texture_target);

  // Now draw some stuff on top of it to show we really did something
#define WIDTH 200
#define HEIGHT 100
  glBegin(GL_QUADS);
  glColor3f(1.0f, 0, 0); //Set the colour to red
  glVertex2f(10, 10);
  glVertex2f(10, HEIGHT * scale);
  glVertex2f(WIDTH * scale, HEIGHT * scale);
  glVertex2f(WIDTH * scale, 10);
  glEnd();

  // done; clean up.
  glPopAttrib();

  // release the data pointers
  if(sourceImg)
    gOpenGLSuite->clipFreeTexture(sourceImg);
  if(outputImg)
    gOpenGLSuite->clipFreeTexture(outputImg);

  return status;
}

// convenience function to define parameters
static void
defineParam( OfxParamSetHandle effectParams,
	     const char *name,
	     const char *label,
	     const char *scriptName,
	     const char *hint,
	     const char *parent)
{
  OfxPropertySetHandle props;
  gParamHost->paramDefine(effectParams, kOfxParamTypeDouble, name, &props);

  // say we are a scaling parameter
  gPropHost->propSetString(props, kOfxParamPropDoubleType, 0, kOfxParamDoubleTypeScale);
  gPropHost->propSetDouble(props, kOfxParamPropDefault, 0, 1.0);
  gPropHost->propSetDouble(props, kOfxParamPropMin, 0, 0.0);
  gPropHost->propSetDouble(props, kOfxParamPropDisplayMin, 0, 0.0);
  //gPropHost->propSetDouble(props, kOfxParamPropDisplayMax, 0, 100.0);
  gPropHost->propSetDouble(props, kOfxParamPropIncrement, 0, 0.01);
  gPropHost->propSetString(props, kOfxParamPropHint, 0, hint);
  gPropHost->propSetString(props, kOfxParamPropScriptName, 0, scriptName);
  gPropHost->propSetString(props, kOfxPropLabel, 0, label);
  if(parent)
    gPropHost->propSetString(props, kOfxParamPropParent, 0, parent);
}

//  describe the plugin in context
static OfxStatus
describeInContext( OfxImageEffectHandle  effect,  OfxPropertySetHandle inArgs)
{
  // get the context from the inArgs handle
  char *context;
  gPropHost->propGetString(inArgs, kOfxImageEffectPropContext, 0, &context);
  //bool isGeneralContext = strcmp(context, kOfxImageEffectContextGeneral) == 0;

  OfxPropertySetHandle props;
  // define the single output clip in both contexts
  gEffectHost->clipDefine(effect, kOfxImageEffectOutputClipName, &props);

  // set the component types we can handle on out output
  gPropHost->propSetString(props, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
  gPropHost->propSetString(props, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentAlpha);

  // define the single source clip in both contexts
  gEffectHost->clipDefine(effect, kOfxImageEffectSimpleSourceClipName, &props);

  // set the component types we can handle on our main input
  gPropHost->propSetString(props, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
  gPropHost->propSetString(props, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentAlpha);

  ////////////////////////////////////////////////////////////////////////////////
  // define the parameters for this context
  // fetch the parameter set from the effect
  OfxParamSetHandle paramSet;
  gEffectHost->getParamSet(effect, &paramSet);

  // overall scale param
  defineParam(paramSet, "scale", "scale", "scale",
	      "Scales the red rect", 0);
  defineParam(paramSet, "source_scale", "source_scale", "source_scale",
	      "Scales the source image", 0);

  // make a page of controls and add my parameters to it
  gParamHost->paramDefine(paramSet, kOfxParamTypePage, "Main", &props);
  gPropHost->propSetString(props, kOfxParamPropPageChild, 0, "scale");
  gPropHost->propSetString(props, kOfxParamPropPageChild, 1, "source_scale");
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

  gOpenGLSuite =
    (OfxImageEffectOpenGLRenderSuiteV1 *)gHost->fetchSuite(gHost->host, kOfxOpenGLRenderSuite, 1);

  // record a few host features
  gPropHost->propGetInt(gHost->host, kOfxImageEffectPropSupportsMultipleClipDepths, 0, &gHostSupportsMultipleBitDepths);

  // get the property handle for the plugin
  OfxPropertySetHandle effectProps;
  gEffectHost->getPropertySet(effect, &effectProps);

  // We can render both fields in a fielded images in one hit if there is no animation
  // So set the flag that allows us to do this
  gPropHost->propSetInt(effectProps, kOfxImageEffectPluginPropFieldRenderTwiceAlways, 0, 0);

  // say we can support multiple pixel depths and let the clip preferences action deal with it all.
  gPropHost->propSetInt(effectProps, kOfxImageEffectPropSupportsMultipleClipDepths, 0, 1);

  // set the bit depths the plugin can handle
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthByte);
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthShort);
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 2, kOfxBitDepthFloat);

  // set some labels and the group it belongs to
  gPropHost->propSetString(effectProps, kOfxPropLabel, 0, "OFX OpenGL Example");
  gPropHost->propSetString(effectProps, kOfxImageEffectPluginPropGrouping, 0, "OFX Example");

  // define the contexts we can be used in
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 1, kOfxImageEffectContextGeneral);

  // we support OpenGL rendering (could also say "needed" here)
  gPropHost->propSetString(effectProps, kOfxImageEffectPropOpenGLRenderSupported, 0, "true");

  {
    char *s = NULL;
    stat = gPropHost->propGetString(gHost->host, kOfxImageEffectPropOpenGLRenderSupported, 0, &s);
    DPRINT(("Host has OpenGL render support: %s (stat=%d)\n", s, stat));
    gHostSupportsOpenGL = stat == 0 && !strcmp(s, "true");
  }

  // we support all the OpenGL bit depths
  gPropHost->propSetString(effectProps, kOfxOpenGLPropPixelDepth, 0, kOfxBitDepthByte);
  gPropHost->propSetString(effectProps, kOfxOpenGLPropPixelDepth, 1, kOfxBitDepthShort);
  gPropHost->propSetString(effectProps, kOfxOpenGLPropPixelDepth, 2, kOfxBitDepthFloat);

  return kOfxStatOK;
}

////////////////////////////////////////////////////////////////////////////////
// The main function
static OfxStatus
pluginMain(const char *action,  const void *handle, OfxPropertySetHandle inArgs,  OfxPropertySetHandle outArgs)
{
  try {
  // cast to appropriate type
  OfxImageEffectHandle effect = (OfxImageEffectHandle) handle;

  if(strcmp(action, kOfxActionDescribe) == 0) {
    return describe(effect);
  }
  else if(strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
    return describeInContext(effect, inArgs);
  }
  else if(strcmp(action, kOfxActionLoad) == 0) {
    return onLoad();
  }
  else if(strcmp(action, kOfxActionUnload) == 0) {
    return onUnLoad();
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
  else if(strcmp(action, kOfxActionInstanceChanged) == 0) {
    return instanceChanged(effect, inArgs, outArgs);
  }
  else if(strcmp(action, kOfxImageEffectActionGetTimeDomain) == 0) {
    return getTemporalDomain(effect, inArgs, outArgs);
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
  "com.genarts:OpenGLSamplePlugin",
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
