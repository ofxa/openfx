.. _ImageEffectArchitectures:

Image Processing Architectures
==============================

OFX supports a range of image processing architectures. The simpler ones
being special cases of the most complex one. Levels of support, in both
plug-in and host, are signalled by setting appropriate properties in the
plugin and host.

This chapter describes the most general architecture that OFX can
support, with simpler cases just being specialisations of the general
case.

The Image Plane
---------------

At it's most generalised, OFX allows for a complex imaging architecture
based around an infinite 2D plane on which we are filling in pixels.

Firstly, there is some subsection of this infinite plane that the user
wants to be the end result of their work, call this the project extent.
The project extent is always rooted, on its bottom left, at the origin
of the image plane. The project extent defines the upper right hand
corner of the project window. For example a PAL sized project spans (0,
0) to (768, 576) on the image plane.

We define an image effect as something that can fill in a rectangle of
pixels in this infinite plane, possibly using images defined at other
locations on this image plane.

.. _rod:
Regions of Definition
---------------------

An effect has a **Region of Definition** (RoD), this is is the maximum
area of the plane that the effect can fill in. for example: a 'read
source media' effect would only be able to fill an area as big as it's
source media. An effect's RoD may need to be based on the RoD of its
inputs, for example: the RoD of a contrast/brightness colour corrector
would generally be the RoD of it's input, while the RoD of a rotation
effect would be bigger than that of it's input image.

The purpose of the
:c:macro:`kOfxImageEffectActionGetRegionOfDefinition`
action is for the host to ask an effect what its region of definition
is. An effect calculates this by looking at its input clips and the
values of its current parameters.

Hosts are not obliged to render all an effects RoD, as it may have fixed
frame sizes, or any number of other issues.

Infinite RoDs
~~~~~~~~~~~~~

Infinite RoDs are used to indicate an effect can fill pixels in anywhere
on the image plane it is asked to. For example a no-input noise
generator that generates random colours on a per pixel basis. An
infinite RoD is flagged by setting the minimums to be:

.. doxygendefine:: kOfxFlagInfiniteMin

and the maxmimums to be:

.. doxygendefine:: kOfxFlagInfiniteMax


for both double and integer rects. Hosts and plug-ins need to be
infinite RoD aware. Hosts need to clip such RoDs to an appropriate
rectangle, typically the project extent. Plug-ins need to check for
infinite RoDs when asking input clips for them and to pass them through
unless they explicitly clamp them. To indicate an infinite RoD set it as
indicated in the following code snippet.

    ::

              outputRoD.x1 = kOfxFlagInfiniteMin;
              outputRoD.y1 = kOfxFlagInfiniteMin;
              outputRoD.x2 = kOfxFlagInfiniteMax;
              outputRoD.y2 = kOfxFlagInfiniteMax;
            

.. _roi:
Regions Of Interest
-------------------

An effect will be asked to fill in some region of this infinite plane.
The section it is being asked to fill in is called the **Region of
Interest** (RoI).

Before an effect has been asked to process a given RoI, it will be asked
to specify the area of each input clip it will need to process that
area. For example: a simple colour correction effect only needs as much
input as it does output, while a blur will need an area that is larger
than the specified RoI by a border of the same width as the blur radius.

The purpose of the
:c:macro:`kOfxImageEffectActionGetRegionsOfInterest`
action is for the host to ask an effect what areas it needs from each
input clip, to render a specific output region. An effect needs to
examine its set of parameters and the region it has been asked to render
to determine how much of *each* input clip it needs.


Tiled Rendering
---------------

Tiling is the ability of an effect to manage images that are less than
full frame (or in our current nomenclature, less than the full Region of
Definition). By tiling the images it renders, a host will render an
effect in several passes, say by doing the bottom half, then the top
half.

Hosts may tile rendering for a variety of reasons. Usually it is in an
attempt to reduce memory demands or to distribute rendering of an effect
to several different CPUs or computers.

Effects that in effect only perform per pixel calculations (for example
a simple colour gain effect) tile very easily. However in the most
general case for effects, tiling may be self defeating, as an effect, in
order to render a tile, may need significantly more from its input clips
than the tile in question. For example, an effect that performs an 2D
transform on its input image, may need to sample all that image even
when rendering a very small tile on output, as the input image may have
been scaled down so that it only covers a few pixels on output.


Tree Based Architectures
------------------------

The most general compositing hosts allow images to be of any size at any
location on our image plane. They also plumb the output of effects into
other effects, to create effect trees. When evaluating this tree of
effects, a general host will want to render the minimum number of pixels
it needs to fill in the final desired image. Typically the top level of
this compositing tree is being rendered at a certain project size, for
example PAL SD, 2K film and so on. This is where the RoD/RoI calls come
in handy.

The host asks the top effect how much picture information it can
produce, which in turn asks effects below it their RoDs and so on until
leaf effects are reached, which report back up the tree until the top
effect calculates its RoD and reports back to the host. The host
typically clips that RoD to its project size.

Having determined in this way the window it wants rendered at the top
effect, the host asks the top node the regions of interest on each of
it's inputs. This again propagates down the effect tree until leaf nodes
are encountered. These regions of interest are cached at effect for
later use.

At this point the host can start rendering, from the bottom of the tree
upwards, by asking each effect to fill in the region of interest that
was previously specified in the RoI walk. These regions are then passed
to the next level up to render and so on.

Another complication is tiling. If a host tiles, it will need to walk
the tree and perform the RoI calculation for each tile that it renders.

The details may differ on specific hosts, but this is more or less the
most generic way compositing hosts currently work.


Simpler Architectures
---------------------

The above architecture is quite complex, as the inputs supplied can lie
anywhere on the image plane, as can the output, and they can be
subsections of the 'complete' image. Not all hosts work in this way,
generally it is only the more advance compositing systems working on
large resolution images.

Some other systems allow for images to be anywhere on the image plane,
but always pass around full RoD images, never tiles.

The simplest systems, don't have any of of the above complexity. The
RoDs, RoIs, images and project sizes in such systems are exactly the
same, always. Often these are editing, as opposed to compositing,
systems.

Similarly, some plugin effects cannot handle sub RoD images, or even
images not rooted at the origin.

The OFX architecture is meant to support all of them. Assuming a plugin
supports the most general architecture, it will trivially run on hosts
with simpler architectures. However, if a plugin does not support tiled,
or arbitrarily positioned images, they may not run cleanly on hosts that
expect them to do so.

To this end, two properties are provided that flag the capabilities of a
plugin or host...

-  :c:macro:`kOfxImageEffectPropSupportsMultiResolution`
   which indicates support for images of differing sizes not centred
   on the origin,
-  :c:macro:`kOfxImageEffectPropSupportsTiles`
   which indicates support for images that contain less than full
   frame pixel data

A plug-in should flag these appropriately, so that hosts know how to
deal with the effect. A host can either choose to refuse to load a
plugin, or, preferentially, pad images with an appropriate amount of
black/transparent pixels to enable them to work.

The :c:macro:`kOfxImageEffectActionGetRegionsOfInterest` is redundant for
plugins that do not support tiled rendering, as the plugin is asking
that it be given the full Region of Definition of all its inputs. A host
may have difficulty doing this (for example with an input that is
attached to an effect that can create infinite images such as a random
noise generator), if so, it should clamp images to some a size in some
manner.

The RoD/RoI actions are potentially redundant on simpler hosts. For
example fixed frame size hosts. If a host has no need to call these
actions, it simply should not.
