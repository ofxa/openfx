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

  This plugin will take you through the basics of creating an OFX plugin. It
  exercises the basic 'bootstrapping' machinery of the API to allow a host
  application to instantiate the plugin.

  The plugin itself does no image processing, it is a 'no-op' effect.

  The accompanying guide will explain what is happening in more detail.

  Finally, the plugin can be used to validate whether a host application
  drives the OFX machinery correctly, and will print to stderr any issues
  it finds during the whole plugin life cycle.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  // House keeper to make sure we are loaded and unloaded symmetrically
  bool gInLoadedState = false;
  bool gDescribeCalled = false;
  bool gDescribeInContextCalled = false;
  int  gNumInstancesLiving = 0;

  ////////////////////////////////////////////////////////////////////////////////
  // The first _action_ called after the binary is loaded (three boot strapper functions will be howeever)
  OfxStatus LoadAction(void)
  {
    ERROR_ABORT_IF(gHost == NULL, "The OfxHost pointer has not been set, it should have been set in 'setHostFunc' before any action is called.");
    ERROR_ABORT_IF(gInLoadedState == true, "kOfxActionLoad called more than once without an intervening kOfxUnloadAction.");

    /// now fetch a suite out of the host via it's fetch suite function.
    gPropertySuite = (OfxPropertySuiteV1 *) gHost->fetchSuite(gHost->host, kOfxPropertySuite, 1);
    ERROR_ABORT_IF(gPropertySuite == 0, "Failed to fetch the " kOfxPropertySuite " version 1 from the host.");

    gImageEffectSuite = (OfxImageEffectSuiteV1 *) gHost->fetchSuite(gHost->host, kOfxImageEffectSuite, 1);

    // increment count, ideally this should not be called more than once. Should that be a condition? FIXME
    gInLoadedState = true;

    return kOfxStatOK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // last action called before a plugin binary is unloaded.
  OfxStatus UnloadAction(void)
  {
    // make sure no instances have been left alive
    ERROR_IF(gNumInstancesLiving != 0, "kOfxActionUnload called while there are still %d instances of the plugin extant.", gNumInstancesLiving);

    // check that we had a load called first
    ERROR_IF(gInLoadedState != true, "kOfxActionUnload callewd without preceding kOfxAcrtionLoad.");
    gInLoadedState = false;

    return kOfxStatOK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // the plugin's basic description routine
  OfxStatus DescribeAction(OfxImageEffectHandle descriptor)
  {
    // check stuff
    ERROR_ABORT_IF(gInLoadedState != true, "kOfxActionLoad has not been called");
    gDescribeCalled = true;

    // get the property set handle for the plugin
    OfxPropertySetHandle effectProps;
    gImageEffectSuite->getPropertySet(descriptor, &effectProps);

    // set some labels and the group it belongs to
    gPropertySuite->propSetString(effectProps, kOfxPropLabel, 0, "OFX Basics Example");
    gPropertySuite->propSetString(effectProps, kOfxImageEffectPluginPropGrouping, 0, "OFX Example");

    // define the image effects contexts we can be used in, in this case a simple filter
    gPropertySuite->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);

    return kOfxStatOK;
  }

  //  describe the plugin in context
  OfxStatus
  DescribeInContextAction(OfxImageEffectHandle descriptor,  OfxPropertySetHandle inArgs)
  {
    // check state
    ERROR_ABORT_IF(gDescribeCalled == false, "DescribeInContextAction called before DescribeAction");
    gDescribeInContextCalled = true;

    // get the context from the inArgs handle
    char *context;
    gPropertySuite->propGetString(inArgs, kOfxImageEffectPropContext, 0, &context);

    ERROR_IF(strcmp(context, kOfxImageEffectContextFilter) != 0, "DescribeInContextAction called on unsupported contex %s", context);

    OfxPropertySetHandle props;
    // define the mandated single output clip
    gImageEffectSuite->clipDefine(descriptor, "Output", &props);

    // set the component types we can handle on out output
    gPropertySuite->propSetString(props, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gPropertySuite->propSetString(props, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentAlpha);

    // define the mandated single source clip
    gImageEffectSuite->clipDefine(descriptor, "Source", &props);

    // set the component types we can handle on our main input
    gPropertySuite->propSetString(props, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gPropertySuite->propSetString(props, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentAlpha);

    return kOfxStatOK;
  }

  //  instance construction
  OfxStatus CreateInstanceAction(OfxImageEffectHandle instance)
  {
    // check stuff
    ERROR_ABORT_IF(gDescribeInContextCalled == false, "CreateInstanceAction called before DescribeInContext.");
    ++gNumInstancesLiving;

    OfxPropertySetHandle effectProps;
    gImageEffectSuite->getPropertySet(instance, &effectProps);

    // attach some instance data to the effect handle, it can be anything
    char *myString = strdup("This is random instance data that could be anything you want.");

    // set my private instance data
    gPropertySuite->propSetPointer(effectProps, kOfxPropInstanceData, 0, (void *) myString);

    return kOfxStatOK;
  }

  // instance destruction
  OfxStatus DestroyInstanceAction(OfxImageEffectHandle instance)
  {
    --gNumInstancesLiving;

    OfxPropertySetHandle effectProps;
    gImageEffectSuite->getPropertySet(instance, &effectProps);

    // get my private instance data
    char *myString = NULL;
    gPropertySuite->propGetPointer(effectProps, kOfxPropInstanceData, 0, (void **) &myString);
    ERROR_IF(myString == NULL, "Instance should not be null!");
    free(myString);

    return kOfxStatOK;
  }

  // are the settings of the effect making it redundant and so not do anything to the image data
  OfxStatus IsIdentityAction(OfxImageEffectHandle instance,
                             OfxPropertySetHandle inArgs,
                             OfxPropertySetHandle outArgs)
  {
    // we set the name of the input clip to pull data from
    gPropertySuite->propSetString(outArgs, kOfxPropName, 0, "Source");
    return kOfxStatOK;
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

    else if(strcmp(action, kOfxActionUnload) == 0) {
      // The very last action called.
      returnStatus = UnloadAction();
    }
    else if(strcmp(action, kOfxActionDescribe) == 0) {
      // the first action called to describe what the plugin does
      returnStatus = DescribeAction(effect);
    }
    else if(strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
      // the second action called to describe what the plugin does
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
      // The action called by the host to see if the plugin is currently
      // a "no-op". In this example we are always a no-op.
      returnStatus = IsIdentityAction(effect, inArgs, outArgs);
    }

    MESSAGE(": END action is : %s \n", action );
    /// other actions to take the default value
    return returnStatus;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Class that will be used to check that the plugin is correctly unloaded.
  struct CheckNotStillLoaded {
    CheckNotStillLoaded()
    {
    }

    ~CheckNotStillLoaded()
    {
      ERROR_IF(gInLoadedState == true, "kOfxActionUnload has not been called before a plugin binary was dynamically unloaded.");
    }
  };

  // The destructor of CheckNumTimesLoadedIsZero will be called automatically when the plugin dso is unloaded.
  // we use that to check that kOfxActionUnload has been called.
  CheckNotStillLoaded gCheckNotStillLoaded;

} // end of anonymous namespace


////////////////////////////////////////////////////////////////////////////////
// The plugin struct passed back to the host application to initiate bootstrapping\
// of plugin communications
static OfxPlugin effectPluginStruct =
{
  kOfxImageEffectPluginApi,                // The API this plugin satisfies.
  1,                                       // The version of the API it satisifes.
  "org.openeffects:BasicsExamplePlugin",   // The unique ID of this plugin.
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
  ERROR_IF(nth > 0, "Host tried to get more plugins from binary than were available, called with index %d", nth);
  return 0;
}
