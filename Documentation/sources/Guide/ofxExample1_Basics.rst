.. _basicExample:

This is a guide to the basic machinery an OFX plugin uses to communicate
with a host application, and goes into the fundamentals of the API.

An example plugin will be used to illustrate how all the machinery
works, and its source can be found in the C++ file
`there <https://github.com/ofxa/openfx/blob/master/Guide/Code/Example1/basics.cpp>`_.
This plugin is a *no-op* image
effect and does absolutely nothing to images, it is there purely to show
you the basics of how a host and plugin work together. I’ll embed
snippets of the plugin, but with some comments and debug code stripped
for clarity.

An OFX plugin is a compiled dynamic library that an application can load
on demand to add extra features to itself. A standardised API is used by
a host and a plugin to communicate and do what is needed.

OFX has an underlying plugin mechanism that could be used to create a
wide variety of plugin APIs, but currently only one has been layered on
top of the base plugin machinery, which is the OFX Image Effect API.

The OFX API is specified using the ``C`` programming language purely by
a set of header files, there are no libraries a plugin need to link
against to make a plugin or host work. [1]_

Key Concepts and Terminology
============================

OFX has several key concepts and quite specific terminology, which
definitely need defining.

-  a **host** is an application than can load OFX plugins and
   provides an environment for plugins to work in,

-  a **plugin** provides a set of extra features to a host
   application,

-  a **binary** is a dynamic library  [2]_ that contains one or more
   plugins,

-  a **suite** is ``C struct`` containing a set of function
   pointers, which are named and versioned, Suites are the way a host
   allows a plugin to call functions within it and not have to link
   against anything,

-  a **property** is a named object of a restricted set of ``C``
   types, which is accessed via a property suite,

-  a **property set** is a collection of properties,

-  an **action** is a call into a plugin to do something,

-  an **API** is a collection of suites, actions and properties that
   are used to do something useful, like process images. APIs are named
   and versioned.

.. _bootstrapperFunctions:

The Two Bootstrapper Functions
==============================

To tell the host what it has inside it, a plugin binary needs to expose
two functions to bootstrap the whole host/plugin communications process.
These are:

-  :ref:`OfxGetNumberOfPlugins()<OfxGetNumberOfPlugins>`  A function that returns the
   number of plugins within that binary

-  :ref:`OfxGetPlugin()<OfxGetPlugin>` A function that returns a
   ``struct`` that provides the information required by a host to
   bootstrap the plugin.

The host should load the binary using the appropriate operating system
calls, then search it for these two exposed symbols. It should then
iterate over the number of advertised plugins and decide what to do with
the plugins it finds.

It should go without saying that a host should not hang onto the pointer
returned by :ref:`OfxGetPlugin()<OfxGetPlugin>` after it unloads a binary, as the data will
not be valid. It should also copy any strings out of the struct if it
wants to keep them.

From our example, we have the following…

`basics.cpp <https://github.com/ofxa/openfx/blob/doc/Documentation/sources/Guide/Code/Example1/basics.cpp#L343>`_

.. code:: c++

    // how many plugins do we have in this binary?
    int OfxGetNumberOfPlugins(void)
    {
      return 1;
    }

    // return the OfxPlugin struct for the nth plugin
    OfxPlugin * OfxGetPlugin(int nth)
    {
      if(nth == 0)
        return &effectPluginStruct;
      return 0;
    }

The OfxPluginStruct
===================

The :ref:`OfxPlugin<OfxPlugin>` returned by :ref:`OfxGetPlugin()<OfxGetPlugin>` provides
information about the implementation of a particular plugin.

The fields in the struct give the host enough information to uniquely
identify the plugin, what it does, and what version it is. These are:

-  **pluginAPI** - the name of the API that this plugin satisfies,
    image effect plugins should set this to :c:macro:`kOfxImageEffectPluginApi`,

-  **apiVersion** - the version of that API the plug-in was written
   to

-  **pluginIdentifier** - the unique name of the plug-in. Used only
   to disambiguate the plug-in from all other plug-ins, not necessarily
   for human eyes

-  **pluginVersionMajor** - the major version of the plug-in,
   typically incremented when compatibility breaks,

-  **pluginVersionMinor** - the minor version of the plug-in,
   typically incremented when bugs and so on are fixed,

-  **setHost** - a function used to set the :ref:`OfxHost struct <OfxHost>` in the
   plugin,

-  **mainEntry** - the function a host will use to send action
   requests to the plugin.

Our example plugin’s ``OfxPlugin`` struct looks like…

`basics.cpp <https://github.com/ofxa/openfx/blob/doc/Documentation/sources/Guide/Code/Example1/basics.cpp#L326>`_

.. code:: c++

    static OfxPlugin effectPluginStruct =
    {
      kOfxImageEffectPluginApi,
      1,
      "org.openeffects:BasicsExamplePlugin",
      1,
      0,
      SetHostFunc,
      MainEntryPoint
    };

Using this information a host application can grab a plugin struct then
figure out if it supports the API at the given version.

The **pluginIdentifier** is not meant to be the presented to the
user, it is a purely a unique id for that plugin, *and any related
versions* of that plugin. Use this for serialisation etc... to identify
the plugin. The domainname:pluginname nomenclature is suggested best
practice for a unique id. For a user visible name, use the
:c:macro:`kOfxPropVersionLabel` property

Plugin versioning allows a plugin (as identified by the
**pluginIdentifier** field) to be updated and redistributed multiple
times, with the host knowing which is the most appropriate version to
use. It even allows old and new versions of the same plugin to be used
simultaneously within a host application. There are more details on how
to use the version numbers in the OFX Programming Reference.

The **setHost** function is used by the host to give the plugin an
:ref:`OfxHost<OfxHost>` struct (see below), which is the bit that gives the
plugin access to functions within the host application.

Finally the :ref:`mainEntry<mainEntryPoint>` is the function called by the host to get
the plugin to carry out actions. Via the property system it behaves as a
generic function call, allowing arbitrary numbers of parameters to be
passed to the plugin.

Suites
======

A suite is simply a struct with a set of function pointers. Each suite
is defined by a C struct definition in an OFX header file, as well a C
literal string that names the suite. A host will pass a set of suites to
a plugin, each suite having the set of function pointers filled
appropriately.

For example, look in the file `ofxMemory.h <https://github.com/ofxa/openfx/blob/master/include/ofxMemory.h>`_ for the suite used to perform
memory allocation:

`ofxMemory.h <https://github.com/ofxa/openfx/blob/master/include/ofxMemory.h#L48>`_

.. doxygenstruct:: OfxMemorySuiteV1

Notice also, the version number built into the name of the memory suite.
If we ever needed to change the memory suite for some reason,
:cpp:class:`OfxMemorySuiteV2` would be defined, with a new set of function
pointers. The new suite could then live along side the old suite to
provide backwards compatibility.

Plugins have to ask for suites from the host by name with a specific
version, how we do that is covered next.


The OfxHost and Fetching Suites
-------------------------------

An instance of an :ref:`OfxHost<OfxHost>` C struct is the thing that allows a
plugin to get suites and provides information about a host application


A plugin is given one of these by the host application via the
:cpp:func:`OfxPlugin::setHost` function it previously passed to the host.

There are two members to an :ref:`OfxHost<OfxHost>`, the first is a property set
(more on properties in a moment) which describes what the host does and
how it behaves.

The second member is a function used to fetch suites from the host
application. Going back to our example plugin, we have the following
bits of code. For the moment ignore how and when the LoadAction is
called, but notice what it does…

.. _LoadActionExample:

`basics.cpp <https://github.com/ofxa/openfx/blob/doc/Documentation/sources/Guide/Code/Example1/basics.cpp#L99>`_

.. code:: c++

    //  The anonymous namespace is used to hide symbols from export.
    namespace {
      OfxHost               *gHost;
      OfxPropertySuiteV1    *gPropertySuite = 0;
      OfxImageEffectSuiteV1 *gImageEffectSuite = 0;

      ////////////////////////////////////////////////////////////////////////////////
      /// call back passed to the host in the OfxPlugin struct to set our host pointer
      void SetHostFunc(OfxHost *hostStruct)
      {
        gHost = hostStruct;
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// the first action called
      OfxStatus LoadAction(void)
      {
        gPropertySuite    = (OfxPropertySuiteV1 *) gHost->fetchSuite(gHost->host,
                                                                     kOfxPropertySuite,
                                                                     1);
        gImageEffectSuite = (OfxImageEffectSuiteV1 *) gHost->fetchSuite(gHost->host,
                                                                        kOfxImageEffectSuite,
                                                                        1);

        return kOfxStatOK;
      }

    }

Notice that it is fetching two suites by name from the host. Firstly the
all important :c:macro:`kOfxPropertySuite` and then the :c:macro:`kOfxImageEffectSuite`. It
squirrels these away for later use in two global pointers. The plugin
can then use the functions in the suites as and when needed.

.. _properties::

Properties
==========

The main way plugins and hosts communicate is via the properties
mechanism. A property is a named object inside a property set, which is
a bit like a python dictionary. You use the property suite, defined in
the header **ofxProperty.h** to access them.

Properties can be of the following fundamental types…

-  ``int``

-  ``double``

-  ``char *``

-  ``void *``

So for in our example we have….

`basics.cpp <https://github.com/ofxa/openfx/blob/doc/Documentation/sources/Guide/Code/Example1/basics.cpp#L160>`_

.. code:: c++

        OfxPropertySetHandle effectProps;
        gImageEffectSuite->getPropertySet(effect, &effectProps);

        gPropertySuite->propSetString(effectProps, kOfxPropLabel, 0, "OFX Basics Example");

Here the plugin is using the effect suite to get the property set on the
effect. It is then setting the string property **kOfxPropLabel** to
be "OFX Basics Example". There are corresponding calls for the other
data types, and equivalent set calls. All pretty straight forwards.

Notice the **0** passed as the third argument, which is an index.
Properties can be multidimensional, for example the current pen position
in a graphics viewport is a 2D integer property. You can get and set
individual elements in a multidimensional property or you could use
calls like **OfxPropertySuiteV1::propSetIntN** to set all values at
once. Of course there exists *N* calls for all types, as well as
corresponding setting calls.

The various OFX header files are littered with C macros that define the
properties used by the API, what type they are, what property set they
are on and whether you can read and/or write them. The OFX reference
guide had all the properties listed by name and object they are on, as
well as what they are for.

By passing information via property sets, rather than fixed C structs,
you gain a flexibility that allows for simple incremental additions to
the API without breaking backwards compatibility and builds. It does
come at a cost (being continual string look-up), but the flexibility it
gives is worth it.

.. note::

    Plugins have to be very careful with scope of the pointer returned
    when you fetch a string property. The pointer will be guaranteed to
    be valid *only* until the next call to an OFX suite function or
    until the action ends. If you want to use the string out of those
    scope you *must* copy it.

Actions
=======

Actions are how a host tells a plugin what to do. The :ref:`mainEntry<mainEntryPoint>`
function pointer in the :ref:`OfxPlugin<OfxPlugin>` structure is the what accepts
actions to do whatever is being requested.

Where:

-  ``action`` is a C string that specifies what is to be done by the
   plugin, e.g. ``OfxImageEffectActionRender`` tells an image effect
   plugin to render a frame

-  ``handle`` is the thing that is being operated on, and needs to be
   downcast appropriately, what this is will depend on the action

-  ``inArgs`` is a well defined property set that are the arguments to
   the action

-  ``outArgs`` is a well defined property set where a plugin can return
   values as needed.

The entry point will return an :cpp:type:`OfxStatus` to tell the host what
happened. A plugin is not obliged to trap all actions, just a certain
subset, and if it doesn’t need to trap the action, it can just return
the status :c:macro:`kOfxStatReplyDefault` to have the host carry out the
well defined default for that action.

So looking at our example we can see its main entry point:

`basics.cpp <https://github.com/ofxa/openfx/blob/doc/Documentation/sources/Guide/Code/Example1/basics.cpp#L259>`_

.. code:: c++

      OfxStatus MainEntryPoint(const char *action,
                               const void *handle,
                               OfxPropertySetHandle inArgs,
                               OfxPropertySetHandle outArgs)
      {
        // cast to appropriate type
        OfxImageEffectHandle effect = (OfxImageEffectHandle) handle;

        OfxStatus returnStatus = kOfxStatReplyDefault;

        if(strcmp(action, kOfxActionLoad) == 0) {
          returnStatus = LoadAction();
        }
        else if(strcmp(action, kOfxActionUnload) == 0) {
          returnStatus = UnloadAction();
        }
        else if(strcmp(action, kOfxActionDescribe) == 0) {
          returnStatus = DescribeAction(effect);
        }
        else if(strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
          returnStatus = DescribeInContextAction(effect, inArgs);
        }
        else if(strcmp(action, kOfxActionCreateInstance) == 0) {
          returnStatus = CreateInstanceAction(effect);
        }
        else if(strcmp(action, kOfxActionDestroyInstance) == 0) {
          returnStatus = DestroyInstanceAction(effect);
        }
        else if(strcmp(action, kOfxImageEffectActionIsIdentity) == 0) {
          returnStatus = IsIdentityAction(effect, inArgs, outArgs);
        }

        return returnStatus;
      }

You can see the plugin is trapping seven actions and is saying to do the
default for the rest of the actions.

In fact only four actions need to be trapped for an image effect plugin
 [3]_, but our machinery plugin is trapping more for illustrative
purposes.

What is on the property sets, and what the handle is depends on the
action being called. Some actions have no arguments (eg: the
:c:macro:`kOfxLoadAction`), while others have in and out arguments, e.g. the
:c:macro:`kOfxImageEffectActionIsIdentity`.

Actions give us a very flexible and expandable generic function calling
mechanism. This means it is trivial to expand the API via adding extra
properties or actions to the API without impacting existing plugins or
applications.

.. note::

    For the main entry point on image effect plugins, the handle passed
    in will either be NULL or an :cpp:type:`OfxImageEffectHandle``, which is
    just a blind pointer to host specific data that represents the
    plugin.


Basic Actions For Image Effect Plugins
======================================

There are a set of actions called on a plugin that signal to the plugin
what is going on and to get it to tell the host what the plugin does.
These need to be called in a specific sequence to make it all work
properly.


The Load and Unload Actions
---------------------------

The :c:macro:`kOfxActionLoad` is the very first action passed to a plugin. It will
be called after the ``setHost`` callback has been used to pass the
:ref:`OfxHost<OfxHost>` to the plugin. It is the point at which a plugin gets to
create global structures that it will later be used across all
instances. From our :ref:`load action snippet <LoadActionExample>` above,
you can see that the plugin is fetching two suites and caching the
pointers away for later use.

At some point the host application will want to unload the binary that
the plugin is contained in, either when the host quits or the plugin is
no longer needed by the host application. The host needs to notify the
plugin of this, as it may need to perform some clean up. The
:c:macro:`kOfxActionUnload` action is sent to the plugin by the host to warn the
plugin of it’s imminent demise. After this action is called the host can
no longer issue any actions to that plugin unless another :c:macro:`kOfxActionLoad`
action is called. In our example plugin, the unload does nothing.

.. note::

    Hosts should always pair the :c:macro:`kOfxActionLoad` with a :c:macro:`kOfxActionUnload`,
    otherwise all sorts of badness can happen, including memory leaks,
    failing license checks and more. There is one exception to this,
    which is if a plugin encounters an error during the load action and
    returns an error state. In this case only, the plugin *must* clean
    up before it returns, and , the balancing unload action is *not*
    called. In all other circumstances where an error is returned by a
    plugin from any other action, the unload action will eventually be
    called.


.. _describingPlugin:

Describing Plugins To A Host
----------------------------

Once a plugin has had :c:macro:`kOfxActionLoad` called on it, it will be asked to
describe itself. This is done with the :c:macro:`kOfxActionDescribe` action. From
our example plugin, here is the function called by our main entry point
in response to the describe action.

`basics.cpp <https://github.com/ofxa/openfx/blob/doc/Documentation/sources/Guide/Code/Example1/basics.cpp#L149>`_

.. code:: c++

      OfxStatus DescribeAction(OfxImageEffectHandle descriptor)
      {
        // get the property set handle for the plugin
        OfxPropertySetHandle effectProps;
        gImageEffectSuite->getPropertySet(descriptor, &effectProps);

        // set some labels and the group it belongs to
        gPropertySuite->propSetString(effectProps,
                                      kOfxPropLabel,
                                      0,
                                      "OFX Basics Example");
        gPropertySuite->propSetString(effectProps,
                                      kOfxImageEffectPluginPropGrouping,
                                      0,
                                      "OFX Example");

        // define the image effects contexts we can be used in, in this case a simple filter
        gPropertySuite->propSetString(effectProps,
                                      kOfxImageEffectPropSupportedContexts,
                                      0,
                                      kOfxImageEffectContextFilter);

        return kOfxStatOK;
      }

You will see that it fetches a property set (via the image effect suite)
and sets various properties on it. Specifically the label used in any
user interface to name the plugin, and the group of plugins it belongs
to. The grouping name allows a developer to ask the host to arrange all
plugins with that group name into a single menu/container in the user
interface.

The final thing it sets is the single context it can be used in.
Contexts are specific to image effect plugins, and they are there
because a plugin can be used in many different ways. We call each way an
image effect plugin can be used a context. In our example we are saying
our plugin can behave as a filter only. A filter is simply an effect
with one and only one input clip and one mandated output clip. This is
typical of systems such as editors which can drop effects directly onto
a clip in a time-line. For more complex systems, e.g. a node graph
compositor, you might want to allow the same plugin to have more input
clips and a richer parameter set, which we call the general context. A
plugin can work one or more contexts, not all of which need be supported
by a host.

Because it can be used in different contexts, and will need to be
described differently in each, an image effect plugin has a two tier
description process. First :c:macro:`kOfxActionDescribe` is called to set
attributes common to all the contexts the plugin can be used in, then
the :c:macro:`kOfxImageEffectActionDescribeInContext` action is called, once for
each context that the host wants to use the effect in.

Again from our example plugin, here is how it responds to the describe
in context action…

.. note::

    A plugin developer might package multiple plugins in a single binary
    and another multiple plugins into multiple binaries yet both expect
    them to show up in the same plugin group  [4]_ in the user
    interface.

`basics.cpp <https://github.com/ofxa/openfx/blob/doc/Documentation/sources/Guide/Code/Example1/basics.cpp#L171>`_

.. code:: c++

      OfxStatus
      DescribeInContextAction(OfxImageEffectHandle descriptor, OfxPropertySetHandle inArgs)
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

In this case I’ve left the error check cluttering up the snippet so you
can see how the ``inArgs`` property set is used to specify which context
is currently being described. Our example then goes on define two image
clips, the first used for output, and the second used for input. The API
docs specify that a filter effect needs to specify both of these with
exactly those names. Not also how the effect is setting a
multidimensional property associated with each clip to specify what
pixel types it supports on those clips.

For more complex effects, these actions are the point where you specify
parameters that the effect wants to use, and get to tweak a whole range
of settings to say how the plugin behaves.

.. _createInstance::

Creating Instances
------------------

So far a host knows what our plugin looks like and how it should behave,
but it isn’t using it to process pixels yet. At some point a user will
click on a button in a UI and to say they want to use the plugin. To do
that a host creates an *instance* of the plugin. An instance represents
a unique copy of the plugin and contains all the state needed for that.
For example, a blur plugin may be instantiated many times in a
compositing graph, each instance will have parameters set to a different
value, and be connected to different input and output clips.

A plugin developer may need to attach data to each plugin instance,
typically to tie the plugin into their own image processing
infrastructure. They get the chance to do that via the
:c:macro:`kOfxActionCreateInstance` action. The host will call that action just
after they have created and initialised their host-side data structures
that represent the plugin. Our example plugin doesn’t actually do
anything on create instance, but it could choose to attached it’s own
data structures to the instance via the :c:macro:`kOfxPropInstanceData`
property.

A plugin will also want to destroy any of its own data structures when
an instance is destroyed. It gets to do that in the
:c:macro:`kOfxActionDestroyInstance` action.

Our example plugin exercises both of those action just to illustrate
what is going it. It simply places a string into the instance data
property which it later fetches and destroys. In real plugins, this is
typically a hook to deeper plugin side data structures.

.. note::

    Because a host might have asynchronous UI handling and multiple
    render threads on the same instance, it is suggested that a plugin
    that wants to write to the instance data after instance creation do
    so in a safe manner (e.g. by semaphore lock).

`basics.cpp <https://github.com/ofxa/openfx/blob/doc/Documentation/sources/Guide/Code/Example1/basics.cpp#L202>`_

.. code:: c++

      OfxStatus CreateInstanceAction(OfxImageEffectHandle instance)
      {
        OfxPropertySetHandle effectProps;
        gImageEffectSuite->getPropertySet(instance, &effectProps);

        // attach some instance data to the effect handle, it can be anything
        char *myString = strdup("This is random instance data that could be anything you want.");

        // set my private instance data
        gPropertySuite->propSetPointer(effectProps,
                                       kOfxPropInstanceData,
                                       0,
                                       (void *) myString);

        return kOfxStatOK;
      }

      // instance destruction
      OfxStatus DestroyInstanceAction(OfxImageEffectHandle instance)
      {
        OfxPropertySetHandle effectProps;
        gImageEffectSuite->getPropertySet(instance, &effectProps);

        // get my private instance data
        char *myString = NULL;
        gPropertySuite->propGetPointer(effectProps,
                                       kOfxPropInstanceData,
                                       0,
                                       (void **) &myString);
        ERROR_ABORT_IF(myString == NULL, "Instance data should not be null!");
        free(myString);

        return kOfxStatOK;
      }

.. note::

    :c:macro:`kOfxActionDestroyInstance` should always be called when an instance
    is destroyed, and furthermore all instances need to have had
    kOfxActionDestroyInstance called on them before :c:macro:`kOfxActionUnload` can
    be called.


What About The Image Processing?
--------------------------------

This plugin is pretty much a *hello world* OFX example, it doesn’t
actually process any images. Normally a host application would call the
:c:macro:`kOfxImageEffectActionRender` action when it wants the plugin to
render a frame. Our simple plugin gets around processing any images by
trapping the :c:macro:`kOfxImageEffectActionIsIdentity` action. This action
lets the plugin tell the host application that it currently does nothing
to its inputs, for example a blur effect with the blur size of zero. In
such a case the host can simply ignore the plugin and use its source
images directly. And here is the code that does that:

`basics.cpp <https://github.com/ofxa/openfx/blob/doc/Documentation/sources/Guide/Code/Example1/basics.cpp#L238>`_

.. code:: c++

      OfxStatus IsIdentityAction( OfxImageEffectHandle instance,
                                  OfxPropertySetHandle inArgs,
                                  OfxPropertySetHandle outArgs)
      {
        // we set the name of the input clip to pull data from
        gPropertySuite->propSetString(outArgs, kOfxPropName, 0, "Source");
        return kOfxStatOK;
      }

The plugin is telling the host to pass through an unprocessed image from
an input clip, and because plugins can have more than one input it needs
to tell the host which clip to use. It does that by setting the
:c:macro:`kOfxPropName` property on the outargs. It also returns
:c:macro:`kOfxStatOK` to indicate that it has trapped the action and that
the plugin is currently doing nothing.

Remember we said that each action has a well defined set of in and out
arguments? In the case of the is identity action these are…

-  :c:macro:`kOfxPropTime` - the time at which to test for identity

-  :c:macro:`kOfxImageEffectPropFieldToRender` - the field to test for identity

-  :c:macro:`kOfxImageEffectPropRenderWindow` - the window to test for identity
   under

-  :c:macro:`kOfxImageEffectPropRenderScale` - the scale factor being applied to
   the images being rendered

-  :c:macro:`kOfxPropName` this to the name of the clip that should be used if the
   effect is an identity transform, defaults to the empty string

-  :c:macro:`kOfxPropTime` the time to use from the indicated source clip as an
   identity image (allowing time slips to happen), defaults to the value
   in :c:macro:`kOfxPropTime` in inArgs

A proper plugin would examine the inArgs, its parameters and see if it
is doing anything to its inputs. If it does need to process images it
would return :c:macro:`kOfxStatReplyDefault` rather than
:c:macro:`kOfxStatOK``.

.. _lifeCycle::
Life Cycle of a Plugin
======================

Now we’ve outlined the basic actions and functions in a plugin, we
should clearly specify the calling sequence. Failure to call them in the
right sequence will lead to all sorts of undefined behaviour.

Assuming the host has done nothing apart from load the dynamic library
that contains plugins and has found the two :ref:`boostrapping
symbols <bootstrapperFunctions>` in the plugin, the host
should then...

-  call :ref:`OfxGetNumberOfPlugins<OfxGetNumberOfPlugins>` to discover the number of plugins

-  call :ref:`OfxGetPlugin<OfxGetPlugin>` for each of the N plugins in the binary and
   decide if it can use them or not (by looking at APIs and versions)

At this point the code in the binary should have done nothing apart from
run those two functions. The host is free to unload the binary at this
point without further interaction with the plugin.

If the host decides it wants to use one of the plugins in the binary it
must then…

-  call the ``setHost`` function given to it *for that plugin* and pass
   back an OfxHost struct which allows plugins to fetch suites
   appropriate for the API

-  call the :c:macro:`kOfxActionLoad`

-  call :c:macro:`kOfxActionDescribe`

-  call :c:macro:`kOfxImageEffectActionDescribeInContext` for each context

If the host wants to actually use a plugin, it creates whatever host
side data structures are needed then…

-  calls :c:macro:`kOfxActionCreateInstance`

When a host wants to get rid of an instance, before it destroys any of
it’s own data structures it calls :c:macro:`kOfxActionDestroyInstance`

When the host wants to be done with the plugin, and before it
dynamically unloads the binary it calls :c:macro:`kOfxActionUnload`, all
instances *must* have been destroyed before this call.

Once the final :c:macro:`kOfxActionUnload` has been called, even if it doesn’t
dynamically unload the binary, the host can no longer call the main
entry point on that specific plugin until it once more calls
:c:macro:`kOfxActionLoad`.


Packaging A Plugin
------------------

The compiled code for a plugin is contained in a dynamic library.
Plugins are distributed as a directory structure that allows you to add
icons and other resources you may need. There is more detailed
information in the OFX Programming Reference Guide.

Summary
=======

This example has shown you the basics of the OFX plugin machinery, the
main things it illustrated was…

-  the :ref:`two bootstrapper functions <bootstrapperFunctions>`
   exposed by a plugin that start the plugin discovery process,

-  the main entry point of a plugin is given :ref:`actions <ImageEffectActions>` by
   the host application to do things,

-  the plugin gets :ref:`suites <suitesReference>` from the host to gain access to
   functions in the host,

-  :ref:`property sets <properties>` are the main way of passing data back
   and forth across the API,

-  image effect plugins are :ref:`described <describingPlugin>` in a two step process,

-  :ref:`instances are created <createInstance>` when a host wants to
   use a plugin to do something

-  actions must be called in a :ref:`certain
   order <lifeCycle>` for the API to work cleanly.

.. [1]
   Though there exist optional host and plugin support libraries that
   can be used to help you in your coding.

.. [2]
   which will be operating system specific

.. [3]
   :c:macro:`kOfxLoadAction`, :c:macro:`kOfxActionDescribe`,
   :c:macro:`kOfxImageEffectActionDescribeInContext` and one of
    :c:macro:`kOfxImageEffectActionIsIdentity` or :c:macro:`kOfxImageEffectActionRender`

.. [4]
    as specified by :c:macro:`kOfxImageEffectPluginPropGrouping`
