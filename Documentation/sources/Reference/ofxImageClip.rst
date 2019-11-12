.. _imageClip:
Images and Clips
================

What Is An Image?
-----------------

Image Effects process images (funny that), this chapter describes
images and clips of images, how they behave and how to deal with them.

Firstly some definitions...

-  an image is a rectangular array of addressable pixels,
-  a clip is a contiguous sequence of images that vary over time.

Images and clips contain pixels, these pixels can currently be of the
following types...

-  a colour pixel with red, green, blue, alpha components
-  a colour pixel with red, green and blue components
-  single component 'alpha' images

The components of the pixels can be of the following types...

-  8 bit unsigned byte, with the nominal black and white points at 0 and
   255 respectively,
-  16 bit unsigned short, with the nominal black and white points at 0
   and 65535 respectively,
-  32 bit float, with the nominal black and white points at 0.0f and
   1.0f respectively, component values are not clipped to 0.0f and 1.0f.

Components are packed per pixel in the following manner...

-  RGBA pixels as R, G, B, A
-  RGB pixels as R, G, B

There are several structs for pixel types in `ofxCore.h <https://github.com/ofxa/openfx/blob/master/include/ofxCore.h>` that can be used
for raw pixels in OFX.

Images are always left to right, bottom to top, with the pixel data
pointer being at the bottom left of the image. The pixels in a scan line
are contiguously packed.

Scanlines need *not* be contiguously packed. The number of *bytes*
between between a pixel in the same column, but separated by a scan line
is known as the *rowbytes* of an image. Rowbytes can be negative,
allowing for compositing systems with a native top to bottom scanline
order to trivially support bottom to top images.

Clips and images also have a *pixel aspect ratio*, this is how much an
actual addressable pixel must be stretched by in X to be square. For
example PAL SD images have a pixel aspect ratio of 1.06666.

Images are rectangular, whose integral bounds are in :ref:`Pixel coordinates<PixelCoordinates>`,
with the image being X1 <= X < X2 and Y1 <= Y < Y2, ie: exclusive on the
top and right. The bounds represent the amount of data present in the
image, which may be larger, smaller or equal to the Region of Definition
of the image, depending on the architecture supported by the plugin. The
:c:macro:`kOfxImagePropBounds` property on an image
holds this information.

An image also contains it's RoD in image coordinates, in the
:c:macro:`kOfxImagePropRegionOfDefinition`
property. The RoD is the maximum area that an image may have pixels in,
t he bounds are the actual addressable pixels present in an image. This
allows for tiled rendering an so on.

Clips have a frame rate, which is the number of frames per second they
are to be displayed at. Some clips may be continuously samplable (for
example, if they are connected to animating geometry that can be
rendered at arbitrary times), if this is so, the frame rate for these
clips is set to 0.

Images may be composed of full frames, two fields or a single field,
depending on its source and how the effect requests the image be
processed. Clips are either full frame sequences or fielded sequences.

Images and clips also have a premultiplication state, this represents
how the alpha component and the RGB/YUV components may have interacted.


Defining Clips
--------------

During an the effect's describe in context action an effect *must*
define the clips mandated for that context, it can also define extra
clips that it may need for that context. It does this using the
:cpp:func`OfxImageEffectSuiteV1::clipDefine`
function, the property handle returned by this function is purely for
definition purposes only. It has not persistence outside the describe in
context action and is distinct to the clip property handles used by
instances. The *name* parameter is how you can later access that clip in
a plugin instance via the
:cpp:func`OfxImageEffectSuiteV1::clipGetHandle` function.

During the describe in context action, the plugin sets properties on a
clip to control its use. The properties that can be set during a
describe in context call are...

-  :c:macro:`kOfxPropLabel`
   to give a user readable name to the clip (the host need not use this,
   for example in a transition it is redundant),
-  :c:macro:`kOfxImageEffectPropSupportedComponents`
   to specify which components it is willing to accept on that clip,
-  :c:macro:`kOfxImageClipPropOptional`
   to specify if the clip is optional,
-  :c:macro:`kOfxImageClipPropFieldExtraction`
    specifies how to extract fielded images from a clip, see :ref:`this section <ImageEffectsFieldRendering>`
    for more details on field and field rendering
-  :c:macro:`kOfxImageEffectPropTemporalClipAccess`
   whether the effect wants to access images from the clip at times
   other that the frame being renderred.

Plugins *must* indicate which pixel depths they can process by setting
the
:c:macro:`kOfxImageEffectPropSupportedPixelDepths`
on the plugin handle during the describe action.

Pixel Aspect Ratios, frame rates, fielding, components and pixel depths
are constant for the duration of a clip, they cannot changed from frame
to frame.

.. note::

    -  it is an error not to set the :c:macro:`kOfxImageEffectPropSupportedPixelDepths`
       plugin property during its describe action
    -  it is an error not to define a mandated input clip during the
       describe in context action
    -  it is an error not to set the :c:macro:`kOfxImageEffectPropSupportedComponents`
       on an input clip during describe in context

.. _ImageEffectsImagesAndClipsUsingClips:

Getting Images From Clips
-------------------------

Clips in instances are retrieved via the
:cpp:func`OfxImageEffectSuiteV1::clipGetHandle`
function. This returns a property handle for the clip in a specific
instance. This handle is valid for the duration of the instance.

Images are fetched from a clip via the
:cpp:func:`OfxImageEffectSuiteV1::clipGetImage`
function. This takes a time and an optional region to extract an image
at from a given clip. This returns, in a property handle, an image
fetched from the clip at a specific time. The handle contains all the
information relevant to dealing with that image.

Once fetched, an image must be released via the
:cpp:func:`OfxImageEffectSuiteV1::clipReleaseImage`
function. All images must be released within the action they were
fetched in. You cannot retain an image after an action has returned.

Images may be fetched from an attached clip in the following
situations...

-  in the :c:macro:`kOfxImageEffectActionRender` action
-  in the :c:macro:`kOfxActionInstanceChanged`
and :c:macro:`kOfxActionEndInstanceChanged` actions with a :c:macro:`kOfxPropChangeReason` of :c:macro:`kOfxChangeUserEdited`

A host may not be able to support random temporal access, it flags its
ability to do so via the
:c:macro:`kOfxImageEffectPropTemporalClipAccess`
property. A plugin that wishes to perform random temporal access must
set a property of that name on the plugin handle and the clip it wishes
to perform random access from.

.. note::

    -  it is an error for a plugin to attempt random temporal image access if the host does not support it
    -  it is an error for a plugin to attempt random temporal image access
   if it has not flagged that it wishes to do so and the clip it wishes
   to do so from.

.. _ImageEffectsPremultiplication:

Premultiplication And Alpha
---------------------------

All images and clips have a premultiplication state. This is used to
indicate how the image should interpret RGB (or YUV) pixels, with
respect to alpha. The premultiplication state can be...

.. doxygendefine:: kOfxImageOpaque

The image is opaque and so has no premultiplication state, but the
alpha component in all pixels is set to the white point

.. doxygendefine:: kOfxImagePreMultiplied

The image is premultiplied by it's alpha

.. doxygendefine:: kOfxImageUnPreMultiplied

The image is unpremultiplied.

This document won't go into the details of premultiplication, but will
simply state that OFX takes notice of it and flags images and clips
accordingly.

The premultiplication state of a clip is constant over the entire
duration of that clip.

.. _ImageEffectsPixelAspectRatios:

Clips and Pixel Aspect Ratios
-----------------------------

All clips and images have a pixel aspect ratio, this is how much a
'real' pixel must be stretched by in X to be square. For example PAL D1
images have a pixel aspect ratio of 1.06666.

The property
:c:macro:`kOfxImageEffectPropSupportsMultipleClipPARs`
is used to control how a plugin deals with pixel aspect ratios. This is
both a host and plugin property. For a host it can be set to...

-  0 - the host only supports a single pixel aspect ratio for all clips,
   input or output, to an effect,
-  1 - the host can support differing pixel aspect ratios for inputs and
   outputs

For a plugin it can be set to...

-  0 - the plugin expects all pixel aspect ratios to be the same on all
   clips, input or output
-  1 - the plugin will accept clips of differing pixel aspect ratio.

If a plugin does not accept clips of differing PARs, then the host must
resample all images fed to that effect to agree with the output's PAR.

If a plugin does accept clips of differing PARs, it will need to specify
the output clip's PAR in the
:c:macro:`kOfxImageEffectActionGetClipPreferences`
action.

.. _ImageEffectsMemoryAllocation:

Allocating Your Own Images
--------------------------

Under OFX, the images you fetch from the host have already had their
memory allocated. If a plug-in needs to define its owns temporary images
buffers during processing, or to cache images between actions, then the
plug-in should use the image memory allocation routines declared in
:cpp:class:`OfxImageEffectSuiteV1`. The reason for this is that many host have
special purpose memory pools they manage to optimise memory usage as
images can chew up memory very rapidly (eg: a 2K RGBA floating point
film plate is 48 MBytes).

For general purpose (as in less than a megabyte) memory allocation, you
should use the memory suite in ofxMemory.h

OFX provides four functions to deal with image memory. These are,

-  :cpp:func:`OfxImageEffectSuiteV1::imageMemoryAlloc`
-  :cpp:func:`OfxImageEffectSuiteV1::imageMemoryFree`
-  :cpp:func:`OfxImageEffectSuiteV1::imageMemoryLock`
-  :cpp:func:`OfxImageEffectSuiteV1::imageMemoryUnlock`

A host needs to be able defragment its image memory pool, potentially
moving the contents of the memory you have allocated to another address,
even saving it to disk under its own virtual memory caching scheme.
Because of this when you request a block of memory, you are actually
returned a handle to the memory, not the memory itself. To use the
memory you must first lock the memory via the imageMemoryLock call,
which will then return a pointer to the locked block of memory.

During an single action, there is generally no need to lock/unlock any
temporary buffers you may have allocated via this mechanism. However
image memory that is cached between actions should always be unlocked
while it is not actually being used. This allows a host to do what it
needs to do to optimise memory usage.

Note that locks and unlocks nest. This implies that there is a lock
count kept on the memory handle, also not that this lock count cannot be
negative. So unlocking a completely unlocked handle has no effect.

An example is below....

    ::

          // get a memory handle
          OfxImageMemoryHandle memHandle;
          gEffectSuite->imageMemoryAlloc(0, imageSize, &memHandle);

          // lock the handle and get a pointer
          void *memPtr;
          gEffectSuite->imageMemoryLock(memHandle, &memPtr);
          
          ... // do stuff with our pointer

          // now unlock it
          gEffectSuite->imageMemoryUnlock(memHandle);

          
          // lock it again, note that this may give a completely different address to the last lock
          gEffectSuite->imageMemoryLock(memHandle, &memPtr);
          
          ... // do more stuff

          // unlock it again
          gEffectSuite->imageMemoryUnlock(memHandle);

          // delete it all
          gEffectSuite->imageMemoryFree(memHandle);
