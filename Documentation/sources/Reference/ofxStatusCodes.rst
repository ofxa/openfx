.. _statusCodes:

Status Codes
============

Status codes are returned by most functions in OFX suites and all
plug-in actions to indicate the success or failure of the operation. All
status codes are defined in `ofxCore.h <https://github.com/ofxa/openfx/blob/master/include/ofxCore.h>`_ and 
*#defined* to be integers.

.. doxygentypedef:: OfxStatus


Most OFX functions in host suites and all actions in a plug-in return a
status code, where the status codes are all 32 bit integers. This
typedef is used to label that status code.


.. doxygendefine:: kOfxStatOK

.. doxygendefine:: kOfxStatFailed

.. doxygendefine:: kOfxStatErrFatal

.. doxygendefine:: kOfxStatErrUnknown

.. doxygendefine:: kOfxStatErrMissingHostFeature

.. doxygendefine:: kOfxStatErrUnsupported

.. doxygendefine:: kOfxStatErrExists

.. doxygendefine:: kOfxStatErrFormat

.. doxygendefine:: kOfxStatErrMemory

.. doxygendefine:: kOfxStatErrBadHandle

.. doxygendefine:: kOfxStatErrBadIndex

.. doxygendefine:: kOfxStatErrValue

.. doxygendefine:: kOfxStatReplyYes

.. doxygendefine:: kOfxStatReplyNo

.. doxygendefine:: kOfxStatReplyDefault


