The Generic Core API
====================

This chapter describes how plug-ins are distributed and the core API for
loading and identifying image effect plug-ins, and the methods of
communications between plug-in and host.

.. Core OFX include files:

OFX Include Files
-----------------

The *C* include files that define an OFX API are all that are needed by
a plug-in or host to implement the API. Most include files define a set
of independent *suites* which are used by a plug-in to communicate with
a host application.

There are two include files that are used with nearly every derived API.
These are...

-  `ofxCore.h <https://github.com/ofxa/openfx/blob/master/include/ofxCore.h>`_
   is used to define the basic communication mechanisms between a host
   and a plug-in. This includes the way in which a plug-in is defined to
   a host and how to bootstrap the two way communications. It also has
   several other basic action and property definitions.
-  `ofxProperty.h <https://github.com/ofxa/openfx/blob/master/include/ofxProperty.h>`_
   specifies the property suite, which is how a plug-in gets and sets
   values on various objects in a host application.

Identifying and Loading Plug-ins
--------------------------------

Plug-ins must implement two exported functions for a host to identify
the plug-ins and to initiate the boot strapping of communication between
the two. 

.. _OfxGetNumberOfPlugins:

.. doxygenfunction:: OfxGetNumberOfPlugins

.. _OfxGetPlugin:

.. doxygenfunction:: OfxGetPlugin



``OfxGetNumberOfPlugins`` is the very first function called by the host after the binary has been
loaded.
The returned pointer to OfxGetPlugin and pointers in the struct do not need to be freed in any way by the host.
   

.. _mainEntryPoint:

The Plug-in Main Entry Point And Actions
----------------------------------------

Actions are how a host communicates with a plug-in. They are in effect
generic function calls. Actions are issued via a plug-in's ``mainEntry``
function pointer found in its :ref:`OfxPlugin struct<OfxPlugin>`. The function
signature for the main entry point is

.. doxygentypedef:: OfxPluginEntryPoint


The :ref:`OfxStatus<statusCodes>` value returned is dependent upon the action being
called, however the value :c:macro:`kOfxStatReplyDefault` is returned if the
plug-in does not trap the action.

The exact set of actions passed to a plug-in's entry point are dependent
upon the API the plug-in implements. However, there exists a core set of
generic actions that most APIs would use.

Suites
------

Suites are how a plug-in communicates back to the host. A suite is
simply a set of function pointers in a C struct. The set of suites a
host needs to implement is defined by the API being implemented. A suite
is fetched from a host via the :cpp:func:`OfxHost::fetchSuite` function. This
returns a pointer (cast to ``void *``) to the named and versioned set of
functions. By using this suite fetching mechanism, there is no symbolic
dependency from the plug-in to the host, and APIs can be easily
expandable without causing backwards compatibility issues.

If the host does not implement a requested suite, or the requested
version of that suite, then it should return NULL.

.. include:: ofxLoadingSequence.rst

Who Owns The Data?
------------------

Objects are passed back and forth across the API, and in general, it is
the thing that passes the data that is responsible for destroying it.
For example the property set handle in the
:ref:`OfxHost struct<OfxHost>` is managed by the host.

There are a few explicit exceptions to this. For example, when an image
effect asks for an image from a host it is passed back a property set
handle which represents the image. That handle needs to later be
disposed of by an effect, by an explicit function call back to the host.
These few exceptions are documented with the suite functions that access
the object.

.. ArchitectureStrings:

Strings
~~~~~~~

A special case is made for strings. Strings are considered to be of two
types, *value* strings and *label* strings. A label string is any string
used by OFX to name a property or type. A value string is generally a
string value of a property.

More specifically, a label string is a string passed across the API as
one of the following...

*  a property label (i.e: the char* property argument in the property
   suites)
*  a string argument to a suite function which must be one of a set of
   predefined set of values e.g:
   paramType
   argument to
   OfxParameterSuiteV1::paramDefine
   , but not the
   name
   argument)

Label strings are considered to be static constant strings. When passed
across the API the host/plug-in receiving the string neither needs to
duplicate nor free the string, it can simply retain the original pointer
passed over and use that in future, as it will not change. A host must
be aware that when it unloads a plug-in all such pointers will be
invalid, and be prepared to cope with such a situation.

A value string is a string passed across the API as one of the
following...

-  all value arguments to any of the property suite calls
-  any other char* argument to any other function.

Value strings have no assumptions made about them. When one is passed
across the API, the thing that passed the string retains ownership of
it. The thing getting the string is not responsible for freeing that
string. The scope of the string's validity is until the next OFX API
function is called. For example, within a plugin

    ::

           // pointer to store the returned value of the host name
           char *returnedHostName;

           // get the host name
           propSuite->propGetString(hostHandle, kOfxPropName, 0, &returnedHostName);

           // now make a copy of that before the next API call, as it may not be valid after it
           char *hostName = strdup(returnedHostName);

           paramSuite->getParamValue(instance, "myParam", &value);
            
