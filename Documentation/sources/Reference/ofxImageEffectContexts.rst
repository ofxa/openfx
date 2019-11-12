.. _imageEffectContexts:
Image Effect Contexts
=====================

How an image effect is used by an end user affects how it should
interact with a host application. For example an effect that is to be
used as a transition between two clips works differently to an effect
that is a simple filter. One must have two inputs and know how much to
mix between the two input clips, the other has fewer constraints on it.
Within OFX we have standardised several different uses and have called
them *contexts*.

More specifically, a context mandates certain behaviours from an effect
when it is described or instantiated in that context. The major issue is
the number of input clips it takes, and how it can interact with those
input clips.

All OFX contexts have a single output clip and zero or more input clips.
The current contexts defined in OFX are:

-  :c:macro:`kOfxImageEffectContextGenerator`

 No compulsory input clips used by a host to create imagery from scratch, e.g: a noise generator

-  :c:macro:`kOfxImageEffectContextFilter`

   A single compulsory input clip. A traditional 'filter effect' that transforms a single input in
      some way, e.g: a simple blur

-  :c:macro:`kOfxImageEffectContextTransition`

   Two compulsory input clips and a compulsory 'Transition' double parameter
   Used to perform transitions between clips, typically in editing
      applications, eg: a cross dissolve,

-  :c:macro:`kOfxImageEffectContextPaint`

   Two compulsory input clips, one image to paint onto, the other a mask to control where the effect happens
   Used by hosts to use an effect under a paint brush

-  :c:macro:`kOfxImageEffectContextRetimer`

   A single compulsory input clip, and a compulsory 'SourceTime' double parameter
   Used by a host to change the playback speed of a clip,

-  :c:macro:`kOfxImageEffectContextGeneral`

   An arbitrary number of inputs, generally used in a 'tree' compositing environment, a catch all context.

A host or plug-in need not support all contexts. For example a host that
does not have any paint facility within it should not need to support
the paint context, or a simple blur effect need not support the retimer
context.

An effect may say that it can be used in more than one context, for
example a blur effect that acts as a filter, with a single input to
blur, and a general effect, with an input to blur and an optional input
to act as a mask to attenuate the blur. In such cases a host should
choose the most appropriate context for the way that host's
architecture. With our blur example, a tree based compositing host
should simply ignore the filter context and always use it in the general
context.

Plugins and hosts inform each other what contexts they work in via the
multidimensional
:c:macro:`kOfxImageEffectPropSupportedContexts`
property.

A host indicates which contexts it supports by setting the
:c:macro:`kOfxImageEffectPropSupportedContexts` property in the global host
descriptor. A plugin indicates which contexts it supports by setting
this on the effect descriptor passed to the
:c:macro:`kOfxActionDescribe` action.

Because a plugin can work in different ways, it needs the ability to
describe itself to the host in different ways. This is the purpose of
the
:c:macro:`kOfxImageEffectActionDescribeInContext`
action. This action is called once for each context that the effect
supports, and the effect gets to describe the input clips and parameters
appropriate to that context. This means that an effect can have
different sets of parameters and clips in different contexts, though it
will most likely have a core set of parameters that it uses in all
contexts. From our blur example, both the filter and general contexts
would have a 'blur radius' parameter, but the general context might have
an 'invert matte' parameter.

During the :c:macro:`kOfxImageEffectActionDescribeInContext` action, an effect
must describe all clips and parameters that it intends to use. This
includes the mandated clips and parameters for that context.

A plugin instance is created in a specific contex which will not changed
over the lifetime of that instance. The context can be retrieved from
the instance via the
:c:macro:`kOfxImageEffectPropContext`
property on the instance handle.

.. _generatorContext:

The Generator Context
---------------------

A generator context is for cases where a plugin can create images
without any input clips, eg: a colour bar generator.

In this context, a plugin has the following mandated clips,

-  an output clip named *Output*

Any input clips that are specified must be optional.

A host is responsible for setting the initial preferences of the output
clip, it must do this in a manner that is transparent to the plugin. So
the pixel depths, components, fielding, frame rate and pixel aspect
ratio are under the control of the host. How it arrives at these is a
matter for the host, but as a plugin specifies what components it can
produce on output, as well as the pixel depths it supports, the host
must choose one of these.

Generators still have Regions of Definition. This should generally be,

-  based on the project size eg: an effect that renders a 3D sky
   simulation,
-  based on parameter settings eg: an effect that renders a circle in an
   arbitrary location,
-  infinite, which implies the effect can generate output anywhere on
   the image plane.

The pixel preferences action is constrained in this context by the
following,

-  a plugin cannot change the component type of the *Output* clip,

.. _filterContext:

The Filter Context
------------------

A filter effect is the ordinary way most effects are used with a single
input. They allow track or layer based hosts that cannot present extra
input to use an effect.

In this context, a plugin has the following mandated objects...

-  an input clip named *Source*
-  an output clip named *Output*

Other input clips may be described, which must all be optional. However
there is no way to guarantee that all hosts will be able to wire in such
clips, so it is suggested that in cases where effects can take single or
multiple inputs, they expose themselves in the filter context with a
single input and the general context with multiple inputs.

The pixel preferences action is constrained in this context by the
following,

-  a plugin cannot change the component type of the *Output* clip, it
   will always be the same as the *Source* clip,

.. _transitionContext:

The Transition Context
----------------------

Transitions are effects that blend from one clip to another over time,
eg: a wipe or a cross dissolve.

In this context, a plugin has the following mandated objects...

-  an input clip names 'SourceFrom'
-  an input clip names 'SourceTo'
-  an output clip named *Output*
-  a single double parameter called 'Transition' (see
   Mandated Parameters
   )

Any other input clips that are specified must be optional. Though it is
suggested for simplicity's sake that only the two mandated clips be
used.

The 'Transition' parameter cannot be labelled, positioned or controlled
by the plug-in in anyway, it can only have it's value read, which will
have a number returned between the value of 0 and 1. This number
indicates how far through the transition the effect is, at 0 it should
output all of 'SourceFrom', at 1 it should output all of 'SourceTo', in
the middle some appropriate blend.

The pixel preferences action is constrained in this context by the
following,

-  the component types of the "SourceFrom", "SourceTo" and *Output*
   clips will always be the same,
-  the pixel depths of the "SourceFrom", "SourceTo" and *Output* clips
   will always be the same,
-  a plugin cannot change any of the pixel preferences of any of the
   clips.

.. _paintContext:

The Paint Context
-----------------

Paint effects are effects used inside digital painting system, where the
effect is limited to a small area of the source image via a masking
image. Perhaps 'brush' would have been a better choice for the name of
the context.

In this context, a plugin has the following mandated objects...

-  an input clip names *Source*,
-  an input clip names *Brush*, the only component type it supports is
   'alpha',
-  an output clip named *Output*.

Any other input clips that are specified must be optional.

The masking images consists of pixels from 0 to the white point of the
pixel depth. Where the mask is zero the effect should not occur, where
the effect is whitepoint the effect should be 'full on', where it is
grey the effect should blend with the source in some manner.

The masking image may be smaller than the source image, even if the
effect states that it cannot support multi-resolution images.

The pixel preferences action is constrained in this context by the
following,

-  the pixel depths of the *Source*, *Brush* and *Output* clips will
   always be the same,
-  the component type of *Source* and *Output* will always be the same,
-  a plugin cannot change any of the pixel preferences of any of the
   clips.

.. _retimerContext:

The Retimer Context
-------------------

The retimer context is for effects that change the length of a clip by
interpolating frames from the source clip to create an inbetween output
frame.

In this context, a plugin has the following mandated objects...

-  an input clip names *Source*
-  an output clip named *Output*
-  a 1D double parameter named 'SourceTime' (see
   Mandated Parameters
   )

Any other input clips that are specified must be optional.

The 'SourceTime' parameter cannot be labelled, positioned or controlled
by the plug-in in anyway, it can only have it's value read. Its value is
how the source time to maps to the output time. So if the output time is
'3' and the 'SourceTime' parameter returns 8.5 at this time, the
resulting image should be an interpolated between source frames 8 and 9.

The pixel preferences action is constrained in this context by the
following,

-  the pixel depths of the *Source* and *Output* clips will always be
   the same,
-  the component type of *Source* and *Output* will always be the same,
-  a plugin cannot change any of the pixel preferences of any of the
   clips.

.. _generalContext:

The General Context
-------------------

The general context is to some extent a catch all context, but is
generally how a 'tree' effect should be instantiated. It has no
constraints on its input clips, nor on the pixel preferences actions.

In this context, has the following mandated objects...

-  an output clip named *Output*

.. ImageEffectContextMandatedParameters:

Parameters Mandated In A Context
--------------------------------

The retimer and transition context both mandate a parameter be declared,
the double params 'SourceTime' and 'Transition'. The purpose of these
parameters is for the host to communicate with the plug-in, they are
*not* meant to be treated as normal parameters, exposed on the user
plug-in's user interface.

For example, the purpose of a transition effect is to dissolve in some
interesting way between two separate clips, under control of the host
application. Typically this is done on systems that edit. The mandated
'Transition' double pseudo-parameter is not a normal one exposed on the
plug-in UI, rather it is the way the host indicates how far through the
transition the effect is. For example, think about two clips on a time
line based editor with a transition between them, the host would set the
value value of the 'Transition' parameter implicitly by how far the
frame being rendered is from the start of the transition, something
along the lines of...

    ::

            Transition = (currrentFrame - startOfTransition)/lengthOfTransition;

This means that the host is completely responsible for any user
interface for that parameter, either implicit (as in the above editing
example) or explicit (with a curve).

Similarly with the 'SourceTime' double parameter in the retimer context.
It is up to the host to provide a UI for this, either implicitly (say by
stretching a clip's length on the time line) or via an explicit curve.
Note that the host is not limited to using a UI that exposes the
'SourceTime' as a curve, alternately it could present a 'speed'
parameter, and integrate that to derive a value for 'SourceTime'.
