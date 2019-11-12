Structure of The OFX and the Image Effect API
=============================================

The Structure Of The Generic OFX API
------------------------------------

OFX is actually several things. At its base it is a generic plug-in
architecture which can be used to implement a variety of plug-in APIs.
The first such API to be implemented on the core architecture is the OFX
Image Effect Plug-in API.

It is all specified using the 'C' programming language. C and C++ are
the languages mainly used to write visual effects applications (the
initial target for OFX plug-in APIs) and have a very wide adoption
across most operating systems with many available compilers. By making
the API C, rather than C++, you remove the whole set of problems around
C++ symbol mangling between the host and plug-in.

APIs are defined in OFX by only a set of C header files and associated
documentation. There are no binary libraries for a plug-in or host to
link against.

Hosts rely on two symbols within a plug-in, all other communication is
boot strapped from those two symbols. The plug-in has no symbolic
dependencies from the host. This minimal symbolic dependency allows for
run-time determination of what features to provide over the API, making
implementation much more flexible and less prone to backwards
compatibility problems.

Plug-ins, via the two exposed symbols, indicate the API they implement,
the version of the API, their name, their version and their main entry
point.

A host communicates with a plug-in via sending 'actions' to the
plug-in's main entry function. Actions are C strings that indicate the
specific operation to be carried out. They are associated with sets of
properties, which allows the main entry function to behave as a generic
function.

A plug-in communicates with a host by using sets of function pointers
given it by the host. These sets of function pointers, known as
'suites', are named via a C string and a version number. They are
returned on request from the host as pointers within a C struct.

Properties are typed value/name pairs that exist on the various OFX
objects and are action argument values to the plug-in's main entry
point. They are how a plug-in and host pass individual values back and
forth to each other. The property suite, defined inside
`ofxProperty.h <https://github.com/ofxa/openfx/blob/master/include/ofxProperty.h>`_
is used to do this.

OFX APIs
--------

An OFX plug-in API is a named set of actions, properties and suites to
perform some specific set of tasks. The first such API that has been
defined on the OFX core is the OFX Image Effect API. The set of actions,
properties and suites that constitute the API makes up the major part of
this document.

Various suites and actions have been defined for the OFX image effect
API, however many are actually quite generic and could be reused by
other APIs. The property suite definitely has to be used by all other
APIs, while the memory allocation suite, the parameter suite and several
others would probably be useful for all other APIs. For example the
parameter suite could be re-used to specify user visible parameters to
the other APIs.

Several types are common to all OFX APIs, and as such are defined in
`ofxCore.h <https://github.com/ofxa/openfx/blob/master/include/ofxCore.h>`_.
Most objects passed back to a plug-in are generally
specified by blind data handles, for example:

.. doxygentypedef:: OfxPropertySetHandle

This allows for strong typing on functions but
allows the implementation of the object to be hidden from the plug-in.

*  :ref:`OfxStatus<statusCodes>`

   Used to define a set of status codes indicating the success or
   failure of an action or suite function
   
*  :ref:`OfxHost<OfxHost>`
    
   A C struct that is used by a plug-in to get access to suites from a
   host and properties about the host
   
*  .. doxygentypedef:: OfxPluginEntryPoint

   A typedef for functions used as main entry points for a plug-in
   (and several other objects),
   
*  :ref:`OfxPlugin<OfxPlugin>`
    
   A C struct that a plug-in fills in to describe itself to a host.


Several general assumptions have been made about how hosts and plug-ins
communicate, which specific APIs *are* allowed to break. The main is the
distinction between...

*  Descriptors
   
   Which hosts and plug-ins use to define the general behaviour of an
   object, e.g. the object used to specify what bit depths an Image
   Effect Plug-in is willing to accept,
   
*  Instances
   
    Which hosts and plug-ins use to control the behaviour of a specific
   **live** object.
   

In most APIs descriptors are typically passed from a host to a plug-in
during the :c:macro:`kOfxActionDescribe` action, whilst all other actions
are passed an instance, e.g: the object passed to the
:c:macro:`kOfxActionCreateInstance` action.

The OFX Image Effect API.
-------------------------

The OFX Image Effect Plug-in API is designed for image effect plug-ins
for 2D visual effects. This includes such host applications as
compositors, editors, rotoscoping tools and colour grading systems.

At heart the image effect API allows a host to send a plug-in a set of
images, state the value of a set of parameters and get a resulting image
back. However how it does this is somewhat complicated, as the plug-in
and host have to negotiate what kind of images are handled, how they can
be processed and much more.
