.. _ImageEffectClipPreferences:

Image Effect Clip Preferences
=============================

The
:c:macro:`kOfxImageEffectActionGetClipPreferences`
action is passed to an effect to allow a plugin to specify how it wishes
to deal with it's input clips and to set properties in it's output clip.
This is especially important when there are multiple inputs of which my
have differing properties, eg; pixel depth.

More specifically, there are six properties that can be set during the
clip preferences action, some on the input clip, some on the output
clip, some on both. These are...

-  the depth of a clip's pixels, input or output clip,
-  the components of a clip's pixels, input or output clip,
-  the pixel aspect ratio of a clip, input or output clip,
-  the frame rate of the output clip,
-  the fielding of the output clip,
-  the premultiplication state of the output clip,
-  whether the output clip varys from frame to frame, even if no
   parameters or input images change over time,
-  whether the output clip can be sampled at sub-frame times and produce
   different images.

The behaviour specified by OFX means that a host may need to cast images
from their native data format into one suitable for the plugin. It is
better that the host do any of this pixel shuffling because,

-  the behaviour is orthogonal for all plugins on that host,
-  the code is not replicated in all plugins,
-  the host can optimise the pixel shuffling in one pass with any other
   data grooming it may need to do.

A plugin gets to assert its clip preferences in several situations.
Firstly whenever a clip is attached to a plugin, secondly whenever one
of the parameters in the plugin property
:c:macro:`kOfxImageEffectPropClipPreferencesSlaveParam`
has its value changed. The clip preferences action is never called until
all non optional clips have been attached to the plugin.

.. note::

    -  these properties cannot animate over the duration of an effect
    -  that the ability to set input and output clip preferences is restricted by the context of an effect
    -  optional input clips do not have any context specific restrictions on plugin set preferences.

.. _ImageEffectClipPreferencesFrameVarying:
Frame Varying Effects
---------------------

Some plugins can generate differing output frames at different times,
even if no parameters animate or no input images change. The
:c:macro:`kOfxImageEffectFrameVarying`
property set in the clip preferences action is used to flag this.

A counter example is a solid colour generator. If it has no animating
parameters, the image generated at frame 0 will be the same as the image
generated at any other frame. Intellegent hosts can render a single
frame and cache that for use at all other times.

On the other hand, a plugin that generates random noise at each frame,
and seeds its random number generator with the render time, will create
different images at different times. The host cannot render a single
frame and cache that for use at subsequent times.

To differentiate between these two cases the
:c:macro:`kOfxImageEffectFrameVarying` is
used. If set to 1, it indicates that the effect will need to be rendered
at each frame, even if no input images or parameters are varying. If set
to 0, then a single frame can be rendered and used for all times if no
input images or parameters vary. The default value is 0.

Continuously Sampled Effects
----------------------------

Some effects can generate images at non frame boundaries, even if the
inputs to the effect are frame based and there is no animation.

For example a fractal cloud generator whose pattern evolves with a speed
parameter can be rendered at arbitrary times, not just on frame
boundaries. Hosts that are interested in sub frame rendering can
determine this by behaviour by examining the
:c:macro:`kOfxImageClipPropContinuousSamples`
property set in the clip preferences action. By default this is false.

.. note ::

    Implicitly, all retimers effects can be continuously sampled.

Specifying Pixel Depths
-----------------------

Hosts and plugins flag whether whether they can deal with input/output
clips of differing pixel depths via the
:c:macro:`kOfxImageEffectPropSupportsMultipleClipDepths` property.

If the host sets this to 0, then all effect's input and output clips
will always have the same component depth, and the plugin may not remap
them.

If the plugin sets this to 0, then the host will transparently map all
of an effect's input and output clips to a single depth, even if the
actual clips are of differing depths.
In the above two cases, the common component depth chosen will be the
deepest depth of any input clip mapped to a depth the plugin supports
that loses the least precision. eg: if a plugin supported 8 bit and
float images, but the deepest clip attached to it was 16 bit, the host
would transparently map all clips to float.

If both the plugin and host set this to 1, then the plugin can, during
the
:c:macro:`kOfxImageEffectActionGetClipPreferences`,
specify how the host is to map each clip, including the output clip.
Note that this is the only case where a plugin may set the output depth.

The bitdepth must be one of:

* .. doxygendefine:: kOfxBitDepthByte

* .. doxygendefine:: kOfxBitDepthShort

* .. doxygendefine:: kOfxBitDepthHalf

* .. doxygendefine:: kOfxBitDepthFloat

* .. doxygendefine:: kOfxBitDepthNone

Specifying Pixel Components
---------------------

A plugin specifies what components it is willing to accept on a clip via
the
:c:macro:`kOfxImageEffectPropSupportedComponents`
on the clip's descriptor during the
:c:macro:`kOfxImageEffectActionDescribeInContext`
This is one or more of:

* .. doxygendefine:: kOfxImageComponentRGBA

* .. doxygendefine:: kOfxImageComponentRGB

* .. doxygendefine:: kOfxImageComponentAlpha

* .. doxygendefine:: kOfxImageComponentNone

If an effect has multiple inputs, and each can be a range of component
types, the effect may end up with component types that are incompatible
for its purposes. In this case the effect will want to have the host
remap the components of the inputs and to specify the components in the
output.

For example, a general effect that blends two images will have have two
inputs, each of which may be RGBA or A. In operation, if presented with
RGBA on one and A on the other, it will most likely request that the A
clip be mapped to RGBA by the host and the output be RGBA as well.

In all contexts, except for the general context, mandated input clips
cannot have their component types remapped, nor can the output. Optional
input clips can always have their component types remapped.

In the general context, all input clips may be remapped, as can the
output clip. The output clip has it's default components set to be, -
RGBA if any of the inputs is RGBA, - otherwise YUVA if any of the inputs
is YUVA, - otherwise A if the effect has any inputs, - otherwise RGBA if
there are no inputs.

.. note::

    It a host implementation detail as to how a host actually attaches real
    clips to a plugin. For instance, a host may allow a YUVA clip to be
    wired to an input that asks for RGBA only. However it must map the clip
    to RGBA in a manner that is transparent to the plugin. Similarly for any
    other component types that the plugin does not support on an input.

Specifying Pixel Aspect Ratios
------------------------------

Hosts and plugins flag whether whether they can deal with input/output
clips of differing pixel aspect ratios via the
:c:macro:`kOfxImageEffectPropSupportsMultipleClipPARs` property.

If the host sets this to 0, then all effect's input and output clips
will always have the same pixel aspect ratio, and the plugin may not
remap them.

If the plugin sets this to 0, then the host will transparently map all
of an effect's input and output clips to a single pixel aspect ratio,
even if the actual clips are of differring PARs.

In the above two cases, the common pixel aspect ratio chosen will be the
smallest on all the inputs, as this preserves image data.

If *both* the plugin and host set this to 1, then the plugin can, during
:c:macro:`kOfxImageEffectActionGetClipPreferences`,
specify how the host is to map each clip, including the output clip.

Specifying Fielding
-------------------

The
:c:macro:`kOfxImageEffectPropSetableFielding`
host property indicates if a plugin is able to change the fielding of
the output clip from the default.

The default value of the output clip's fielding is host dependent, but
in general,

-  if any of the input clips are fielded, so will the output clip
-  the output clip may be fielded irregardless of the input clips (for
   example, in a fielded project).

If the host allows a plugin to specify the fielding of the output clip,
then a plugin may do so during the
:c:macro:`kOfxImageEffectActionGetClipPreferences`
by setting the property
:c:macro:`kOfxImageClipPropFieldOrder` in
the out args argumment of the action. For example a defielding plugin
will want to indicate that the output is frame based rather than
fielded.

Specifying Frame Rates
-----------------------

The
:c:macro:`kOfxImageEffectPropSetableFrameRate`
host property indicates if a plugin is able to change the frame rate of
the output clip from the default.

The default value of the output clip's frame rate is host dependent, but
in general, it will be based on the input clips frame rates.

If the host allows a plugin to specify the frame rate of the output
clip, then a plugin may do so during the
:c:macro:`kOfxImageEffectActionGetClipPreferences`.
For example a deinterlace plugin that separates both fields from fielded
footage will want to do double the frame rate of the output clip.

If a plugin changes the frame rate, it is effectively changing the
number of frames in the output clip. If our hypothetical deinterlace
plugin doubles the frame rate of the output clip, it will be doubling
the number of frames in that clip. The timing diagram below should help,
showing how our fielded input has been turned into twice the number of
frames on output.

::

       FIELDED SOURCE      0.0 0.5 1.0 1.5 2.0 2.5 3.0 3.5 4.0 4.5 ....
       DEINTELACED OUTPUT  0   1   2   3   4   5   6   7   8   9 

The mapping of the number of output frames is...

::

        nFrames' = nFrames * FPS' / FPS 

-  nFrames is the default number of frames,
-  nFrames' is the new number of output frames,
-  FPS is the default frame rate,
-  FPS' is the new frame rate specified by a plugin.

Specifying Premultiplication
----------------------------

All clips have a premultiplication state (see `this <http://www.teamten.com/lawrence/graphics/premultiplication/>`_
for a nice explanation).
An effect cannot map the premultiplication state of the
input clips, but it can specify the premultiplication state of the
output clip.

The output's default premultiplication state is...

-  premultiplied if any of the inputs are premultiplied,
-  otherwise unpremultiplied if any of the inputs are unpremultiplied,
-  otherwise opaque.
