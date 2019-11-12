.. CoordinateSystems:
Coordinate Systems
==================

Spatial Coordinates
-------------------

All OFX spatial coordinate systems have the positive Y axis pointing up,
and the positive X axis pointing right.

As stated above, images are simply some rectangle in a potentially
infinite plane of pixels. However, this is an idealisation of what
really goes on, as images composed of real pixels have to take into
account pixel aspect ratios and proxy render scales, as such they will
*not* be in the same space as the image plane. To deal with this, OFX
has three spatial coordinate systems

-  The Canonical Coordinate System
   which describes the idealised image plane
-  The Pixel Coordinate System
   which describes coordinates in addressable pixels
-  The Normalised Canonical Coordinate System
   which allows for resolution independent description of parameters

.. _CanonicalCoordinates:
Canonical Coordinates
~~~~~~~~~~~~~~~~~~~~~

The idealised image plane is always in a coordinate system of square
unscaled pixels. For example a PAL D1 frame occupies (0,0) to (768,576).
We call this the *Canonical Coordinate System*.

Many operations take place in canonical coordinates, parameter values
are expressed in them while the and RoD and RoI actions report their
values back in them.

The Canonical coordinate system is always referenced by double floating
point values, generally via a :cpp:class:`OfxRectD` structure:

.. doxygenstruct:: OfxRectD

.. _PixelCoordinates:
Pixel Coordinates
~~~~~~~~~~~~~~~~~

*Real* images, where we have to deal with addressable pixels in memory,
are in a coordinate system of non-square proxy scaled integer values. So
a PAL D1 image, being renderred as a half resolution proxy would be
(0,0) to (360, 288), which takes into account both the pixel aspect
ratio of 1.067 and a scale factor of 0.5f. We call this the **Pixel
Coordinate System**.

The Pixel coordinate system is always referenced by integer values,
generally via a OfxRectI structure. It is used when referring to
operations on actual pixels, and so is how the bounds of images are
described and the render window passed to the render action.

.. _MappingCoordinates:
Mapping Between The Spatial Coordinate Systems
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To map between the two the pixel aspect ratio and the render scale need
to be known, and it is a simple case of multiplication and rounding.
More specifically, given...

-  pixel aspect ratio,
   PAR
   , found on the image property
   :c:macro:`kOfxImagePropPixelAspectRatio`
-  render scale in X
   SX
   , found on the first dimension of the effect property
   :c:macro:`kOfxImageEffectPropRenderScale`
-  render scale in Y
   SY
   , found on the second dimension of the effect property
   :c:macro:`kOfxImageEffectPropRenderScale`
-  field scale in Y
   FS
   , this is

   -  0.5 if the image property
      :c:macro:`kOfxImagePropField`
      is
      :c:macro:`kOfxImageFieldLower`
      or
      :c:macro:`kOfxImageFieldUpper`
   -  1.0 otherwise.

To map an X and Y coordinates from Pixel coordinates to Canonical
coordinates, we perform the following multiplications...

    ::

          X' = (X * PAR)/SX
          Y' = Y/(SY * FS)
            

To map an X and Y coordinates from Canonical coordinates to Pixel
coordinates, we perform the following multiplications...

    ::

              X' = (X * SX)/PAR
              Y' = Y * SY * FS
            

.. _NormalisedCoordinateSystem:
The Normalized Coordinate System
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Note, normalised parameters and the normalised coordinate system are
being deprecated in favour of :ref:`spatial
parameters <ParameterPropertiesDoubleTypesSpatial>` which can handle
the project rescaling without the problems of converting to/from
normalised coordinates.

On most editing an compositing systems projects can be moved on
resolutions, for example a project may be set up at high definition then
have several versions rendered out at different sizes, say a PAL SD
version, an NTSC SD version and an HD 720p version.

This causes problems with parameters that describe spatial coordinates.
If they are expressed as absolute positions, the values will be
incorrect as the project is moved from resolution to resolution. For
example, a circle drawn at (384,288) in PAL SD canonical coordinates
will be in the centre of the output. Re-render that at 2K film, it will
be in the bottom left hand corner, which is probably not the correct
spot.

To get around this, OFX allows parameters to be flagged as *normalised*,
which is a resolution independent method of representing spatial
coordinates. In this coordinate system, a point expressed as (0.5, 0.5)
will appear in the centre of the screen, always.

To transform between normalised and canonical coordinates a simple
linear equation is required. What that is requires a certain degree of
explanation. It involves three two dimensional values...

-  the project extent
   the resolution of the project, eg: PAL SD
-  the project size
   how much of that is used by imagery, eg: the letter box area in a
   16:9 PAL SD project
-  the project offset
   the bottom left corner of the extent being used, eg: the BL corner of
   a 16:9 PAL SD project

As described above, the project extent is the section of the image plane
that is covered by an image that is the desired output of the project,
so for a PAL SD project you get an extent of 0,0 to 768,576. As the
project is always rooted at the origin, so the extent is actually a
size.

Project sizes and offsets are a bit less obvious. Consider a project
that is going to be output as PAL D1 imagery, the extent will be 0,0 to
768,576. However our example is a letter box 16:9 project, which leaves
a strip of black at bottom and top. The size of the letter box is 768 by
432, while the bottom left of the letter box is offset from the origin
by 0,77. The ASCII art below shows the details.....

    ::

                                                        (768,576) 
                     ---------------------------------------
                     |                                     |
                     |                BLACK                |
                     |.....................................| (768, 504)
                     |                                     |
                     |                                     |
                     |        LETTER BOXED IMAGERY         |
                     |                                     |
                     |                                     |
              (0,72) |.....................................|
                     |                                     |
                     |                BLACK                |
                     |                                     |
                     ---------------------------------------
               (0,0)
            

So in this example...

-  the
   extent
   of the project is the full size of the output image, which is
   768x576,
-  the
   size
   of the project is the size of the letter box section, which is
   768x432,
-  the
   offset
   of the project is the bottom left corner of the project window, which
   is 0,72.

The properties on an effect instance handle allow you to fetch these
values...

-  :c:macro:`kOfxImageEffectPropProjectExtent`
   for the extent of the current project,
-  :c:macro:`kOfxImageEffectPropProjectSize`
   for the size of the current project,
-  :c:macro:`kOfxImageEffectPropProjectOffset`
   for the offset of the current project.

So to map from normalised coordinates to canonical coordinates, you use
the project size and offset...

-  for values that represent a size simply multiply the normalised
   coordinate by the project size
-  for values that represent an absolute position, multiply the
   normalised coordinate by the project size then add the project origin

To flag to the host that a parameter as normalised, we use the
:ref:`kOfxParamPropDoubleType<kOfxParamPropDoubleType>` property.
Parameters that are so flagged have values set and retrieved by an
effect in normalized coordinates. However a host can choose to represent
them to the user in whatever space it chooses. The values that this
property can take are...

-  .. doxygendefine:: kOfxParamDoubleTypeX
   A size in the X dimension dimension (1D only), new for 1.2

-  .. doxygendefine:: kOfxParamDoubleTypeXAbsolute
   A position in the X dimension (1D only), new for 1.2

-  .. doxygendefine:: kOfxParamDoubleTypeY
   A size in the Y dimension dimension (1D only), new for 1.2

-  .. doxygendefine:: kOfxParamDoubleTypeYAbsolute
   A position in the X dimension (1D only), new for 1.2

-  .. doxygendefine:: kOfxParamDoubleTypeXY
   A size in the X and Y dimension (2D only), new for 1.2

-  .. doxygendefine:: kOfxParamDoubleTypeXYAbsolute
   A position in the X and Y dimension (2D only), new for 1.2

-  .. doxygendefine:: kOfxParamDoubleTypeNormalisedX
   Normalised size with respect to the project's X dimension (1D
   only), deprecated for 1.2

-  .. doxygendefine:: kOfxParamDoubleTypeNormalisedXAbsolute
   Normalised absolute position on the X axis (1D only), deprecated
   for 1.2

-  .. doxygendefine:: kOfxParamDoubleTypeNormalisedY
   Normalised size wrt to the project's Y dimension (1D only),
   deprecated for 1.2

-  .. doxygendefine:: kOfxParamDoubleTypeNormalisedYAbsolute
   Normalised absolute position on the Y axis (1D only), deprecated
   for 1.2

-  .. doxygendefine:: kOfxParamDoubleTypeNormalisedXY
   Normalised to the project's X and Y size (2D only), deprecated for
   1.2

-  .. doxygendefine:: kOfxParamDoubleTypeNormalisedXYAbsolute
   Normalised to the projects X and Y size, and is an absolute
   position on the image plane, deprecated for 1.2.

For example, we have an effect that draws a circle. It has two
parameters a 1D double radius parameter and a 2D double position
parameter. It would flag the radius to be
:c:macro:`kOfxParamDoubleTypeNormalisedX`, fetch the value and scale that by
the project size before we render the circle. The host should present
such normalised parameters to the user in a *sensible* range. So for a
PAL project, it would be from 0..768, where the plug-in sees 0..1.

The position can be handled by the
:c:macro:`kOfxParamDoubleTypeNormalisedXYAbsolute` case. In which case the
plugin must scale the parameter's value by the project size and add in
the project offset. This will allow the positional parameter to be moved
between projects transparently.

Temporal Coordinates
--------------------

Within OFX Image Effects, there is only one temporal coordinate system,
this is in output frames referenced to the start of the effect (so the
first affected frame = 0). All times within the API are in that
coordinate system.

All clip instances have a property that indicates the frames for which
they can generate image data. This is
:c:macro:`kOfxImageEffectPropFrameRange`,
a 2D double property, with the first dimension being the first, and the
second being last the time at which the clip will generate data.

Consider the example below, it is showing an effect of 10 frames
duration applied to a clip lasting 20 frames. The first frame of the
effect is in fact the 5th frame of the clip. Both the input and output
have the same frame rate.

    ::

        Effect               0  1  2  3  4  5  6  7  8  9
        Source   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19
              

In this example, if the effect asks for the source image at time '4',
the host will actually return the 9th image of that clip. When queried
the output and source clip instances would report the following...

    ::

                   range[0]     range[1]    FPS
        Output     0            9           25
        Source    -4            15          25
              

Consider the slightly more complex example below, where the output has a
frame rate twice the input's

    ::

        Effect         0  1  2  3  4  5  6  7  8  9
        Source   0     1     2     3     4     5     6     7
              

When queried the output and source clips would report the following.

    ::

                   range[0]     range[1]    FPS
        Output     0            9           50
        Source    -2            12          25
              

Using simple arithmetic, any effect that needs to access a specific
frame of an input, can do so with the formula...

    ::

            f' = (f - range[0]) * srcFPS/outFPS
              
