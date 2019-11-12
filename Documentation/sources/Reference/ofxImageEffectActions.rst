.. _ImageEffectActions:

Actions Passed to An Image Effect
=================================

Actions passed to an OFX Image Effect's plug-in main entry point are
from two categories...

-  actions that could potentially be issued to any kind of plug in, not
   just image effects, known as generic actions, found in
    `ofxCore.h <https://github.com/ofxa/openfx/blob/master/include/ofxCore.h>`_
-  actions that are only applicable purely to image effects, found in
   `ofxImageEffect.h <https://github.com/ofxa/openfx/blob/master/include/ofxImageEffect.h>`_

For generic actions, the ``handle`` passed to to main entry point will
depend on the API being implemented, for all generic actions passed to an
OFX Image Effect plug-in, it will nearly always be an
:cpp:class:`OfxImageEffectHandle`.

Because interacts are a special case, they are dealt with in a separate
chapter, this chapter will deal with actions issued to an image effect
plug-ins main entry point.

.. doxygendefine:: kOfxActionLoad

.. doxygendefine:: kOfxActionUnload

.. doxygendefine:: kOfxActionDescribe

.. doxygendefine:: kOfxActionCreateInstance

.. doxygendefine:: kOfxActionDestroyInstance

.. doxygendefine:: kOfxActionBeginInstanceChanged

.. doxygendefine:: kOfxActionEndInstanceChanged

.. doxygendefine:: kOfxActionInstanceChanged

.. doxygendefine:: kOfxActionPurgeCaches

.. doxygendefine:: kOfxActionSyncPrivateData

.. doxygendefine:: kOfxActionBeginInstanceEdit

.. doxygendefine:: kOfxActionEndInstanceEdit

.. doxygendefine:: kOfxImageEffectActionDescribeInContext

.. doxygendefine:: kOfxImageEffectActionGetRegionOfDefinition

.. doxygendefine:: kOfxImageEffectActionGetRegionsOfInterest

.. doxygendefine:: kOfxImageEffectActionGetFramesNeeded

.. doxygendefine:: kOfxImageEffectActionIsIdentity

.. doxygendefine:: kOfxImageEffectActionRender

.. doxygendefine:: kOfxImageEffectActionBeginSequenceRender

.. doxygendefine:: kOfxImageEffectActionEndSequenceRender

.. doxygendefine:: kOfxImageEffectActionGetClipPreferences

.. doxygendefine:: kOfxImageEffectActionGetTimeDomain

