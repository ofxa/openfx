
Rendering
=========

The :c:macro:`kOfxImageEffectActionRender`
action is passed to plugins, when the host requires them to render an
output frame.

All calls to the
:c:macro:`kOfxImageEffectActionRender` are
bracketed by a pair of
:c:macro:`kOfxImageEffectActionBeginSequenceRender`
and
:c:macro:`kOfxImageEffectActionEndSequenceRender`
actions. This is to allow plugins to prepare themselves for rendering
long sequences by setting up any tables etc.. it may need.

The
:c:macro:`kOfxImageEffectActionBeginSequenceRender`
will indicate the frame range that is to be renderred, and whether this
is purely a single frame render due to interactive feedback from a user
in a GUI.

The render action is used in conjunction with the optional

Identity Effects
----------------

If an effect does nothing to its input clips (for example a blur with
blur size set to '0') it can indicate that it is an identity function
via the
:c:macro:`kOfxImageEffectActionIsIdentity`
action. The plugin indicates which input the host should use for the
region in question. This allows a host to short circuit the processing
of an effect.

Rendering and The Get Region Actions
------------------------------------

Many hosts attempt to minimise the areas that they render by using
regions of interest and regions of definition, while some of the simpler
hosts do not attempt to do so. In general the order of actions, per
frame rendered, is something along the lines of....

-  ask the effect for it's region of definition,
-  clip the render window against that
-  ask the effect for the regions of interest of each of it's inputs
   against the clipped render window,
-  clip those regions of interest against the region of definition of
   each of those inputs,
-  render and cache each of those inputs,
-  render the effect against it's clipped render window.

A host can ask an effect to render an arbitrary window of pixels,
generally these should be clipped to an effect's region of definition,
however, depending on the host, they may not be. The actual region to
render is indicated by the
:c:macro:`kOfxImageEffectPropRenderWindow`
render action argument. If an effect is asked to render outside of its
region of definition, it should fill those pixels in with black
transparent pixels.

Note thate
:cpp:func:`OfxImageEffectSuiteV1::clipGetImage`
function takes an optional *region* parameter. This is a region, in
Canonical coordinates, that the effect would like on that input clip. If
not used in a render action, then the image returned should be based on
the previous get region of interest action. If used, then the image
returned will be based on this (usually be clipped to the input's region
of definition). Generally a plugin should not use the *region* parameter
in the render action, but should leave it to the 'default' region.

.. ImageEffectsMultiThreadingRendering:

Multi-threaded Rendering
------------------------

Multiple render actions may be passed to an effect at the same time. A
plug-in states it's level of render thread safety by setting the
:c:macro:`kOfxImageEffectPluginRenderThreadSafety`
string property. This can be set to one of three states....

.. doxygendefine:: kOfxImageEffectRenderUnsafe

Indicating that only a single 'render' action can be made at any time among all instances

.. doxygendefine:: kOfxImageEffectRenderInstanceSafe

Indicating that any instance can have a single 'render' action at any one time

.. doxygendefine:: kOfxImageEffectRenderFullySafe

Indicating that any instance of a plugin can have multiple renders running simultaneously

.. ImageEffectsSMPRendering:

Rendering in a Symmetric Multi Processing Environment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When rendering on computers that have more that once CPU (or this
new-fangled hyperthreading), hosts and effects will want to take
advantage of all that extra CPU goodness to speed up rendering. This
means multi-threading of the render function in some way.

If the plugin has set
:c:macro:`kOfxImageEffectPluginRenderThreadSafety`
to :c:macro:`kOfxImageEffectRenderFullySafe`, the host may choose to render a
single frame across multiple CPUs by having each CPU render a different
window. However, the plugin may wish to remain in charge of
multithreading a single frame. The plugin set property
:c:macro:`kOfxImageEffectPluginPropHostFrameThreading`
informs the host as to whether the host should perform SMP on the
effect. It can be set to either...

-  1, in which case the host will attempt to multithread an effect
   instance by calling it's render function called simultaneously, each
   call will be with a different renderWindow, but be at the same frame
-  0, in which case the host only ever calls the render function once
   per frame. If the effect wants to multithread it must use the
   OfxMultiThreadSuite API.

A host may have a render farm of computers. Depending exactly how the
host works with it's render farm, it may have multiple copies on an
instance spread over the farm rendering separate frame ranges, 1-100 on
station A, 101 to 200 on station B and so on...

.. ImageEffectsSequentialRendering:

Rendering Sequential Effects
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Some plugins need the output of the previous frame to render the next,
typically they cache some information about the last render and use that
somehow on the next frame. Some temporally averaging degraining
algorithms work that way. Such effects cannot render correctly unless
they are strictly rendered in order, from first to last frame, on a
single instance.

Other plugins are able to render correctly when called in an arbitrary
frame order, but render much more efficiently if rendered in order. For
example a particle system which maintains the state of the particle
system in an instance would simply increment the simulation by a frame
if rendering in-order, but would need to restart the particle system
from scratch if the frame jumped backwards.

Most plug-ins do not have any sequential dependence. For example, a
simple gain operation has no dependence on the previous frame.

Similarly, host applications, due to their architectures, may or may not
be able to guarantee that a plugin can be rendered strictly in-order.
Node based applications typically have much more difficulty in
guaranteeing such behaviour.

To indicate whether a plugin needs to be rendered in a strictly
sequential order, and to indicate whether a host supports such behaviour
we have a property,
:c:macro:`kOfxImageEffectInstancePropSequentialRender`.
For plug-ins this can be one of three values...

-  0, in which case the host can render an instance over arbitrary frame
   ranges on an arbitrary number of computers without any problem
   (default),
-  1, in which case the host must render an instance on a single
   computer over it's entire frame range, from first to last.
-  2, in which case the effect is more efficiently rendered in frame
   order, but can compute the correct result regardless of render
   order.

For hosts, this property takes three values...

-  0, which indicates thet the host can never guarantee sequential
   rendering,
-  1, which indicates thet the host can guarantee sequential rendering
   for plugins that request it,
-  2, which indicates thet the host can sometimes perform sequential
   rendering.

When rendering, a host will set the in args property on
:c:macro:`kOfxImageEffectPropSequentialRenderStatus`
to indicate whether the host is currently supporting sequential renders.
This will be passed to the following actions,

-  the begin sequence render action
-  the sequence render action
-  the end sequence render action

Hosts may still render sequential effects with random frame access in
interactive sessions, for example when the user scrubs the current frame
on the timeline and the host asks an effect to render a preview frame.
In such cases, the plugin can detect that the instance is being
interactively manipulated via the
:c:macro:`kOfxImageEffectPropInteractiveRenderStatus`
property and hack an approximation together for UI purposes. If
eventually rendering the sequence, the host *must* ignore all frames
rendered out of order and not cache them for use in the final result.

A host may set the in args property
:c:macro:`kOfxImageEffectPropRenderQualityDraft`
in :c:macro:kOfxImageEffectActionRender` to ask
for a render in Draft/Preview mode. This is useful for applications that
must support fast scrubbing. These allow a plug-in to take short-cuts
for improved performance when the situation allows and it makes sense,
for example to generate thumbnails with effects applied. For example
switch to a cheaper interpolation type or rendering mode. A plugin
should expect frames rendered in this manner that will not be stuck in
host cache unless the cache is only used in the same draft situations.

.. _ImageEffectsFieldRendering:
OFX : Fields and Field Rendering
--------------------------------

Fields are evil, but until the world decides to adopt sensible video
standard and casts the current ones into the same pit as 2 inch video
tape, we are stuck with them.

Before we start, some nomenclature. The Y-Axis is considerred to be up,
so in a fielded image,

-  even scan lines 0,2,4,6,... are collectively referred to as the lower
   field,
-  odd scan lines 1,3,5,7... are collective referred to as the upper
   field.

We don't call them odd and even, so as to avoid confusion with video
standard, which have scanline 0 at the top, and so have the opposite
sense of our 'odd' and 'even'.

Clips and images from those clips are flagged as to whether they are
fielded or not, and if so what is the spatial/temporal ordering of the
fields in that image. The
:c:macro:`kOfxImageClipPropFieldOrder` clip
and image instance property can be...

.. doxygendefine:: kOfxImageFieldNone

The material is unfielded

.. doxygendefine:: kOfxImageFieldLower

The material is fielded, with scan line 0,2,4.... occurring first in a frame

.. doxygendefine:: kOfxImageFieldUpper

The material is fielded, with scan line 1,3,5.... occurring first in a frame

Images extracted from a clip flag what their fieldedness is with the
property :c:macro:`kOfxImagePropField`, this can
be....

.. doxygendefine:: kOfxImageFieldNone

The image is an unfielded frame

.. doxygendefine:: kOfxImageFieldBoth

The image is fielded and contains both interlaced fields

.. doxygendefine:: kOfxImageFieldLower

The image is fielded and contains a single field, being the lower field (lines 0,2,4...)

.. doxygendefine:: kOfxImageFieldUpper

The image is fielded and contains a single field, being the upper field (lines 1,3,5...)

The plugin specifies how it deals with fielded imagery by setting the
:c:macro:`kOfxImageEffectPluginPropFieldRenderTwiceAlways`
property. This can be,

-  0 - the plugin is to have it's render function called twice only if
   there is animation in any of it's parameters
-  1 - the plugin is to have it's render function called twice always
   (default)

The reason for this is an optimisation. Imagine a text generator with no
animation being asked to render into a fielded output clip, it can treat
an interlaced fielded image as an unfielded frame. So the host can get
the effect to render both fields in one hit and save on the overhead
required to do the rendering in two passes.

If called twice per frame, the time passed to the render action will be
frame and frame+0.5. So 0.0 0.5 1.0 1.5 etc...

When rendering unfielded footage, the host will only ever call the
effect's render action once per frame, with the time being at the
integers, 0.0, 1.0, 2.0 and so on.

The render action's argument property
:c:macro:`kOfxImageEffectPropFieldToRender`
tells the effect which field it should render, this can be one of...

-  :c:macro:`kOfxImageFieldNone`
   - there are no fields to deal with, the image is full frame
-  :c:macro:`kOfxImageFieldBoth`
   - the imagery is fielded and both scan lines should be renderred
-  :c:macro:`kOfxImageFieldLower`
   - the lower field is being rendered (lines 0,2,4...)
-  :c:macro:`kOfxImageFieldUpper`
   - the upper field is being rendered (lines 1,3,5...)

.. note::

    :c:macro:`kOfxImageEffectPropFieldToRender` will be set to :c:macro:`kOfxImageFieldBoth` if
    :c:macro:`kOfxImageEffectPluginPropFieldRenderTwiceAlways` is set to 0 on the plugin

A plugin can specify how it wishes fielded footage to be fetched from a
clip via the clip descriptor property
:c:macro:`kOfxImageClipPropFieldExtraction`.
This can be one of...

-  :c:macro:`kOfxImageFieldBoth`

Fetch a full frame interlaced image

-  :c:macro:`kOfxImageFieldSingle`

Fetch a single field, making a half height image

-  :c:macro:`kOfxImageFieldDoubled`

Fetch a single field, but doubling each line and so making a full
   height image (default)

If fetching a single field, the actual field fetched from the source
frame is...

-  the first temporal field if the time passed to clipGetImage has a
   fractional part of 0.0 <= f < 0.5
-  the second temporal field otherwise,

To illustrate this last behaviour, the two examples below show an output
with twice the frame rate of the input and how clipGetImage maps to the
input. The .0 and .5 mean first and second temporal fields.

    ::

        Behaviour with unfielded footage

        output 0       1       2       3
        source 0       0       1       1
            

    ::

        Behaviour with fielded footage

        output 0.0 0.5 1.0 1.5 2.0 2.5 3.0 3.5            
        source 0.0 0.0 0.5 0.5 1.0 1.0 1.5 1.5
            

NOTE

-  while some rarely used video standards can have odd number of
   scan-lines, under OFX, both fields
   always
   consist of the same number of lines. Pad with black where needed.
-  host developers, for single field extracted images, you don't need to
   do any buffer copies, you just need to set the row bytes property of
   the returned image to twice the normal value, and maybe tweak the
   start address by a scanline.

.. ImageEffectsRenderingAndGUIS:

Rendering In An Interactive Environment
---------------------------------------

Any host with an interface will most likely have an interactive thread
and a rendering thread. This allows an effect to be manipulated while
having renders batched off to a background thread. This will mean that
some degree of locking will go on to prevent simultaneous read/writes
occurring, see :ref:`this section <ImageEffectsThreadSafety>` for more on thread safety.

A host may need to abort a backgrounded render, typically in response to
a user changing a parameter value. An effect should occasionally poll
the :cpp:func:`OfxImageEffectSuiteV1::abort`
function to see if it should give up on rendering.
