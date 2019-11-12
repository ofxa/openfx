Effect Parameters
=================

Introduction
------------

Nearly all plug-ins have some sort of parameters that control their
behaviour, the radius of a circle drawer, the frequencies to filter out
of an audio signal, the colour of a lens flare and so on.

Seeing as hosts already provide for the general management of their own
native parameters (eg: persistence, interface, animation etc...), it
would make no sense to force plug-ins to do this all themselves.

The OFX Parameters Suite is the means by which parameters are defined
and used by the plug-in but maintained by the host. It is defined in the
ofxParam.h header file.

Note that the entire state of the plugin is encoded in the value of its
parameter set. If you need to persist some sort of private data, you
must do so by setting param values in the effects. The :c:macro:`kOfxActionSyncPrivateData`
is an action that tells you
when to flush any values that need persisting out to the effects param
set. You can reconstruct your private data during the :c:macro:`kOfxActionCreateInstance`.

Defining Parameters
-------------------

A plugin needs to define it's parameters during a describe action. It
does this with the
:cpp:func:`OfxParameterSuiteV1::paramDefine`
function, which returns a handle to a parameter *description*.
Parameters cannot currently be defined outside of the plugins describe
actions.

Parameters are uniquely labelled within a plugin with an ASCII null
terminated C-string. This name is not necassarily meant to be end-user
readable, various properties are provided to set the user visible labels
on the param.

All parameters hold properties, though the exact set of properties on a
parameter is dependent on the type of the parameter.

A parameter's handle comes in two slightly different flavours. The
handle returned inside a plugin's describe action is not an actual
instance of a parameter, it is there for the purpose description only.
You can only set properties on that handle (eg: label, min/max value,
default ...), you cannot get values from it or set values in it. The
parameters defined in the describe action will common to all instances
of a plugin.

The handle returned by
:cpp:func:`OfxParameterSuiteV1::paramGetHandle`
outside of a describe action will be a working instance of a parameter,
you can still set (some) properties of the parameter, and all the
get/set value functions are now usable.

.. _parameterTypes:

Parameter Types
---------------

There are seventeen types of parameter. These are

* .. doxygendefine:: kOfxParamTypeInteger
* .. doxygendefine:: kOfxParamTypeInteger2D
* .. doxygendefine:: kOfxParamTypeInteger3D
* .. doxygendefine:: kOfxParamTypeDouble
* .. doxygendefine:: kOfxParamTypeDouble2D
* .. doxygendefine:: kOfxParamTypeDouble3D
* .. doxygendefine:: kOfxParamTypeRGB
* .. doxygendefine:: kOfxParamTypeRGBA
* .. doxygendefine:: kOfxParamTypeBoolean
* .. doxygendefine:: kOfxParamTypeChoice
* .. doxygendefine:: kOfxParamTypeString
* .. doxygendefine:: kOfxParamTypeCustom
* .. doxygendefine:: kOfxParamTypePushButton
* .. doxygendefine:: kOfxParamTypeGroup
* .. doxygendefine:: kOfxParamTypePage
* .. doxygendefine:: kOfxParamTypeParametric


Multidimensional Parameters
---------------------------

Some parameter types are multi dimensional, these are...

-  :c:macro:`kOfxParamTypeDouble2D`

-  :c:macro:`kOfxParamTypeInteger2D`

-  :c:macro:`kOfxParamTypeDouble3D`

-  :c:macro:`kOfxParamTypeInteger3D`

-  :c:macro:`kOfxParamTypeRGB`

-  :c:macro:`kOfxParamTypeRGBA`

-  :c:macro:`kOfxParamTypeParametric`


These parameters are treated in an atomic manner, so that all dimensions
are set/retrieved simultaneously. This applies to keyframes as well.

The non colour parameters have an implicit 'X', 'Y' and 'Z' dimension,
and any interface should display them with such labels.


Integer Parameters
------------------

These are typed by :c:macro:`kOfxParamTypeInteger`, :c:macro:`kOfxParamTypeInteger2D`
and :c:macro:`kOfxParamTypeInteger3D`.

Integer parameters are of 1, 2 and 3D varieties and contain integer
values, between INT_MIN and INT_MAX.

Double Parameters
-----------------

These are typed by :c:macro:`kOfxParamTypeDouble`, :c:macro:`kOfxParamTypeDouble2D`
and :c:macro:`kOfxParamTypeDouble3D`.

Double parameters are of 1, 2 and 3D varieties and contain double
precision floating point values.


Colour Parameters
-----------------

These are typed by :c:macro:`kOfxParamTypeRGB` and :c:macro:`kOfxParamTypeRGBA`.

Colour parameters are 3 or 4 dimensional double precision floating point
parameters. They are displayed using the host's appropriate interface
for a colour. Values are always normalised in the range [0 .. 1], with 0
being the nominal black point and 1 being the white point.


Boolean Parameters
------------------

This is typed by :c:macro:`kOfxParamTypeBoolean`.

Boolean parameters are integer values that can have only one of two
values, 0 or 1.


Choice Parameters
-----------------

This is typed by :c:macro:`kOfxParamTypeChoice`.

Choice parameters are integer values from 0 to N-1, which correspond to
N labeled options.

Choice parameters have their individual options set via the
:c:macro:`kOfxParamPropChoiceOption` property,
for example

    ::

        gPropHost->propSetString(myChoiceParam, kOfxParamPropChoiceOption, 0, "1st Choice");
        gPropHost->propSetString(myChoiceParam, kOfxParamPropChoiceOption, 1, "2nd Choice");
        gPropHost->propSetString(myChoiceParam, kOfxParamPropChoiceOption, 2, "3nd Choice");
        ...
        gPropHost->propSetString(myChoiceParam, kOfxParamPropChoiceOption, n, "nth Choice");

It is an error to have gaps in the choices after the describe action has
returned.


String Parameters
-----------------

This is typed by :c:macro:`kOfxParamTypeString`.

String parameters contain null terminated ``char *`` UTF8 C strings.
They can be of several different variants, which is controlled by the
:c:macro:`kOfxParamPropStringMode` property,
these are

-  .. doxygendefine:: kOfxParamStringIsSingleLine

-  .. doxygendefine:: kOfxParamStringIsMultiLine

-  .. doxygendefine:: kOfxParamStringIsFilePath

-  .. doxygendefine:: kOfxParamStringIsDirectoryPath

-  .. doxygendefine:: kOfxParamStringIsLabel

Group Parameters
----------------

This is typed by :c:macro:`kOfxParamTypeGroup`.

Group parameters allow all parameters to be arranged in a tree
hierarchy. They have no value, they are purely a grouping element.

All parameters have a :c:macro:`kOfxParamPropParent`
property, which is a string property naming the group parameter which is
its parent.

The empty string "" is used to label the root of the parameter
hierarchy, which is the default parent for all parameters.

Parameters inside a group are ordered by their order of addition to that
group, which implies parameters in the root group are added in order of
definition.

Any host based hierarchical GUI should use this hierarchy to order
parameters (eg: animation sheets).

Page Parameters
---------------

This is typed by :c:macro:`kOfxParamTypePage`.

Page parameters are covered in detail in their own
:ref:`section. <ParametersInterfacesPagedLayouts>`

Custom Parameters
-----------------

This is typed by :c:macro:`kOfxParamTypeCustom`.

Custom parameters contain null terminated char \* C strings, and may
animate. They are designed to provide plugins with a way of storing data
that is too complicated or impossible to store in a set of ordinary
parameters.

If a custom parameter animates, it must set its
:c:macro:`kOfxParamPropCustomInterpCallbackV1`
property, which points to a function with the following signature:

.. doxygentypedef:: OfxCustomParamInterpFuncV1

This function is used to interpolate keyframes in custom params.

Custom parameters have no interface by default. However,

-  if they animate, the host's animation sheet/editor should present a
   keyframe/curve representation to allow positioning of keys and
   control of interpolation. The 'normal' (ie: paged or hierarchical)
   interface should not show any gui.
-  if the custom param sets its
   kOfxParamPropInteractV1
   property, this should be used by the host in any normal (ie: paged or
   hierarchical) interface for the parameter.

Custom parameters are mandatory, as they are simply ASCII C strings.
However, animation of custom parameters an support for an in editor
interact is optional.

.. kOfxParamTypePushButton:

Push Button Parameters
----------------------

This is typed by :c:macro:`kOfxParamTypePushButton`.

Push button parameters have no value, they are there so a plugin can
detect if they have been pressed and perform some action. If pressed, a
:c:macro:`kOfxActionInstanceChanged`  action
will be issued on the parameter with a
:c:macro:`kOfxPropChangeReason` of
:c:macro:`kOfxChangeUserEdited`.

Animation
---------

By default the following parameter types animate...

-  :c:macro:`kOfxParamTypeInteger`
-  :c:macro:`kOfxParamTypeInteger2D`
-  :c:macro:`kOfxParamTypeInteger3D`
-  :c:macro:`kOfxParamTypeDouble`
-  :c:macro:`kOfxParamTypeDouble2D`
-  :c:macro:`kOfxParamTypeDouble3D`
-  :c:macro:`kOfxParamTypeRGBA`
-  :c:macro:`kOfxParamTypeRGB`

The following types cannot animate...

-  :c:macro:`kOfxParamTypeGroup`
-  :c:macro:`kOfxParamTypePage`
-  :c:macro:`kOfxParamTypePushButton`

The following may animate, depending on the host. Properties exist on
the host to check this. If the host does support animation on them, then
they do **not** animate by default. They are...

-  :c:macro:`kOfxParamTypeCustom`
-  :c:macro:`kOfxParamTypeString`
-  :c:macro:`kOfxParamTypeBoolean`
-  :c:macro:`kOfxParamTypeChoice`

By default the
:cpp:func:`OfxParameterSuiteV1::paramGetValue`
will get the 'current' value of the parameter. To access values in a
potentially animating parameter, use the
:cpp:func:`OfxParameterSuiteV1::paramGetValueAtTime` function.

Keys can be manipulated in a parameter using a variety of functions,
these are...

-  :cpp:func:`OfxParameterSuiteV1::paramSetValueAtTime`
-  :cpp:func:`OfxParameterSuiteV1::paramGetNumKeys`
-  :cpp:func:`OfxParameterSuiteV1::paramGetKeyTime`
-  :cpp:func:`OfxParameterSuiteV1::paramGetKeyIndex`
-  :cpp:func:`OfxParameterSuiteV1::paramDeleteKey`
-  :cpp:func:`OfxParameterSuiteV1::paramDeleteAllKeys`


Parameter Interfaces
--------------------

Parameters will be presented to the user in some form of interface.
Typically on most host systems, this comes in three varieties...

-  a paged layout, with parameters spread over multiple controls pages
   (eg: the FLAME control pages)
-  a hierarchical layout, with parameters presented in a grouped tree
   (eg: the After Effects 'effects' window)
-  an animation sheet, showing animation curves and key frames.
   Typically this is hierarchical.

Most systems have an animation sheet and present one of either the paged
or the hierarchical layouts.

Because a hierarchy of controls is explicitly set during plugin
definition, the case of the animation sheet and hierarchial GUIs are
taken care of explicitly.

.. _ParametersInterfacesPagedLayouts:

Paged Parameter Editors
-----------------------

A paged layout of controls is difficult to standardise, as the size of
the page and controls, how the controls are positioned on the page, how
many controls appear on a page etc... depend very much upon the host
implementation. A paged layout is ideally best described in the .XML
resource supplied by the plugin, however a fallback page layout can be
specified in OFX via the :c:macro:`kOfxParamTypePage` parameter type.

Several host properties are associated with paged layouts, these are...

-  :c:macro:`kOfxParamHostPropMaxPages`
   The maximum number of pages you may use, 0 implies an unpaged
   layout
-  :c:macro:`kOfxParamHostPropPageRowColumnCount`
   The number of rows and columns for parameters in the paged layout.

Each page parameter represents a page of controls. The controls in that
page are set by the plugin using the :c:macro:`kOfxParamPropPageChild`
multi-dimensional string. For example...

    ::

        OfxParamHandle  page;
        gHost->paramDefine(plugin, kOfxParamTypePage, "Main", &page);
              
        propHost->propSetString(page, kOfxParamPropPageChild, 0, "size");      // add the size parameter to the top left of the page
        propHost->propSetString(page, kOfxParamPropPageChild, 1, kOfxParamPageSkipRow); // skip a row
        propHost->propSetString(page, kOfxParamPropPageChild, 2, "centre");    // add the centre parameter
        propHost->propSetString(page, kOfxParamPropPageChild, 3, kOfxParamPageSkipColumn); // skip a column, we are now at the top of the next column
        propHost->propSetString(page, kOfxParamPropPageChild, 4, "colour"); // add the colour parameter 

The host then places the parameters on that page in the order they were
added, starting at the top left and going down columns, then across rows
as they fill.

Note that there are two pseudo parameters names used to help control
layout:

.. doxygendefine:: kOfxParamPageSkipRow
.. doxygendefine:: kOfxParamPageSkipColumn

These will help control how parameters are added to a page, allowing
vertical or horizontal slots to be skipped.

A host sets the order of pages by using the instance's
:c:macro:`kOfxPluginPropParamPageOrder` property.
Note that this property can vary from context to context, so
you can exclude pages in contexts they are not useful in. For example...

    ::

        OfxStatus describeInContext(OfxImageEffectHandle plugin)
        {
        ...
            // order our pages of controls
            propHost->propSetString(paramSetProp, kOfxPluginPropParamPageOrder, 0, "Main");
            propHost->propSetString(paramSetProp, kOfxPluginPropParamPageOrder, 1, "Sampling");
            propHost->propSetString(paramSetProp, kOfxPluginPropParamPageOrder, 2, "Colour Correction");
            if(isGeneralContext)
               propHost->propSetString(paramSetProp, kOfxPluginPropParamPageOrder, 3, "Dance! Dance! Dance!");
        ...
        } 

.. note::
   Parameters can be placed on more than a single page (this is often useful).
   Group parameters cannot be added to a page.
   Page parameters cannot be added to a page or group.

Instance changed callback
-------------------------

Whenever a parameter's value changes, the host is expected to issue a call to
the :c:macro:`kOfxActionInstanceChanged` action with the name of the parameter
that changed and a reason indicating who triggered the change:

.. doxygendefine:: kOfxChangeUserEdited

.. doxygendefine:: kOfxChangePluginEdited

.. doxygendefine:: kOfxChangeTime

.. ParametersInterfacesUndoRedo:

Parameter Undo/Redo
-------------------

Hosts usually retain an undo/redo stack, so users can undo changes they
make to a parameter. Often undos and redos are grouped together into an
undo/redo block, where multiple parameters are dealt with as a single
undo/redo event. Plugins need to be able to deal with this cleanly.

Parameters can be excluded from being undone/redone if they set the
:c:macro:`kOfxParamPropCanUndo` property to 0.

If the plugin changes parameters values by calling the get and set
value functions, they will ordinarily be put on the undo stack, one
event per parameter that is changed. If the plugin wants to group sets
of parameter changes into a single undo block and label that block, it
should use the
:cpp:func:`OfxParameterSuiteV1::paramEditBegin`
and
:cpp:func:`OfxParameterSuiteV1::paramEditEnd`
functions.

An example would be a 'preset' choice parameter in a sky simulation
whose job is to set other parameters to values that achieve certain
looks, eg "Dusk", "Midday", "Stormy", "Night" etc... This parameter has
a value change callback which looks for :c:macro:`kOfxChangeUserEdited`
then sets other parameters, sky colour, cloud density, sun position
etc.... It also resets itself to the first choice, which says "Example
Skys...".

Rather than have many undo events appear on the undo stack for each
individual parameter change, the effect groups them via the
paramEditBegin/paramEditEnd and gets a single undo event. The 'preset'
parameter would also not want to be undoable as it such an event is
redundant. Note that as the 'preset' has been changed it will be sent
another instance changed action, however it will have a reason of
:c:macro:`kOfxChangePluginEdited`, which it ignores and so stops an infinite
loop occurring.

.. ParametersXML:

XML Resource Specification for Parameters
-----------------------------------------

Parameters can have various properties overridden via a separate XML
based resource file.

.. ParametersPersistance:

Parameter Persistence
---------------------

All parameters flagged with the
:c:macro:`kOfxParamPropPersistant` property will
persist when an effect is saved. How the effect is saved is completely
up to the host, it may be in a file, a data base, where ever. We call a
saved set of parameters a *setup*. A host will need to save the major
version number of the plugin, as well as the plugin's unique identifier,
in any setup.

When an host loads a set up it should do so in the following manner...

1. examines the setup for the major version number.
2. find a matching plugin with that major version number, if multiple
   minor versions exist, the plugin with the largest minor version
   should be used.
3. creates an instance of that plugin with its set of parameters.
4. sets all those parameters to the defaults specified by the plugin.
5. examines the setup for any persistent parameters, then sets the
   instance's parameters to any found in it.
6. calls create instance on the plugin.

It is *not* an error for a parameter to exist in the plugin but not the
setup, and vice versa. This allows a plugin developer to modify
parameter sets between point releases, generally by adding new params.
The developer should be sure that the default values of any new
parameters yield the same behaviour as before they were added, otherwise
it would be a breach of the 'major version means compatibility' rule.

.. ParameterPropertiesVariantTypes:

Parameter Properties Whose Type Vary
------------------------------------

Some properties type depends on the kind of the parameter, eg:
:c:macro:`kOfxParamPropDefault` is an int for a
integer parameter but a double X 2 for a :c:macro:`kOfxParamTypeDouble2D`
parameter.

The variant property types are as follows....

-  :c:macro:`kOfxParamTypeInteger`
    int X 1
-  :c:macro:`kOfxParamTypeDouble`
    double X 1
-  :c:macro:`kOfxParamTypeBoolean`
    int X 1
-  :c:macro:`kOfxParamTypeChoice`
    int X 1
-  :c:macro:`kOfxParamTypeRGBA`
    double X 4 (normalised to 0..1 range)
-  :c:macro:`kOfxParamTypeRGB`
    double X 3 (normalised to 0..1 range)
-  :c:macro:`kOfxParamTypeDouble2D`
    double X 2
-  :c:macro:`kOfxParamTypeInteger2D`
    int X 2
-  :c:macro:`kOfxParamTypeDouble3D`
    double X 3
-  :c:macro:`kOfxParamTypeInteger3D`
    int X 3
-  :c:macro:`kOfxParamTypeString`
    char \* X 1
-  :c:macro:`kOfxParamTypeCustom`
    char \* X 1
-  :c:macro:`kOfxParamTypePushButton`
    none

.. ParameterPropertiesDoubleTypes:

Types of Double Parameters
--------------------------

Double parameters can be used to represent a variety of data, by
flagging what a double parameter is representing, a plug-in allows a
host to represent to the user a more appropriate interface than a raw
numerical value. Double parameters have the
:c:macro:`kOfxParamPropDoubleType` property,
which gives some meaning to the value. This can be one of...

-  .. doxygendefine:: kOfxParamDoubleTypePlain

-  .. doxygendefine:: kOfxParamDoubleTypeAngle

-  .. doxygendefine:: kOfxParamDoubleTypeScale

-  .. doxygendefine:: kOfxParamDoubleTypeTime

-  .. doxygendefine:: kOfxParamDoubleTypeAbsoluteTime

-  .. doxygendefine:: kOfxParamDoubleTypeX

-  .. doxygendefine:: kOfxParamDoubleTypeXAbsolute

-  .. doxygendefine:: kOfxParamDoubleTypeY

-  .. doxygendefine:: kOfxParamDoubleTypeYAbsolute

-  .. doxygendefine:: kOfxParamDoubleTypeXY

-  .. doxygendefine:: kOfxParamDoubleTypeXYAbsolute

-  .. doxygendefine:: kOfxParamDoubleTypeNormalisedX

-  .. doxygendefine:: kOfxParamDoubleTypeNormalisedXAbsolute

-  .. doxygendefine:: kOfxParamDoubleTypeNormalisedY

-  .. doxygendefine:: kOfxParamDoubleTypeNormalisedYAbsolute

-  .. doxygendefine:: kOfxParamDoubleTypeNormalisedXY

-  .. doxygendefine:: kOfxParamDoubleTypeNormalisedXYAbsolute


Plain Double Parameters
-----------------------

Double parameters with their
:c:macro:`kOfxParamPropDoubleType` property set
to :c:macro:`kOfxParamDoubleTypePlain` are uninterpreted. The values
represented to the user are what is reported back to the effect when
values are retrieved. 1, 2 and 3D parameters can be flagged as
:c:macro:`kOfxParamDoubleTypePlain`, which is the default.

For example a physical simulation plugin might have a 'mass' double
parameter, which is in kilograms, which should be displayed and used as
a raw value.


Angle Double Parameters
-----------------------

Double parameters with their
:c:macro:`kOfxParamPropDoubleType` property set
to :c:macro:`kOfxParamDoubleTypeAngle` are interpreted as angles. The host
could use some fancy angle widget in it's interface, representing
degrees, angles mils whatever. However, the values returned to a plugin
are always in degrees. Applicable to 1, 2 and 3D parameters.

For example a plugin that rotates an image in 3D would declare a 3D
double parameter and flag that as an angle parameter and use the values
as Euler angles for the rotation.

Scale Double Parameters
-----------------------

Double parameters with their
:c:macro:`kOfxParamPropDoubleType` property set
to :c:macro:`kOfxParamDoubleTypeScale` are interpreted as scale factors. The
host can represent these as 1..100 percentages, 0..1 scale factors,
fractions or whatever is appropriate for its interface. However, the
plugin sees these as a straight scale factor, in the 0..1 range.
Applicable to 1, 2 and 3D parameters.

For example a plugin that scales the size of an image would declare a
'image scale' parameter and use the raw value of that to scale the
image.


Time Double Parameters
----------------------

Double parameters with their
:c:macro:`kOfxParamPropDoubleType` property set
to :c:macro:`kOfxParamDoubleTypeTime` are interpreted as a time. The host can
represent these as frames, seconds, milliseconds, millennia or whatever
it feels is appropriate. However, a visual effect plugin sees such
values in 'frames'. Applicable only to 1D double parameters. It is an
error to set this on any other type of double parameter.

For example a plugin that does motion blur would have a 'shutter time'
parameter and flags that as a time parameter. The value returned would
be used as the length of the shutter, in frames.


Absolute Time Double Parameters
-------------------------------

Double parameters with their
:c:macro:`kOfxParamPropDoubleType` property set
to :c:macro:`kOfxParamDoubleTypeAbsoluteTime` are interpreted as an absolute
time from the beginning of the effect. The host can represent these as
frames, seconds, milliseconds, millennia or whatever it feels is
appropriate. However, a plugin sees such values in 'frames' from the
beginning of a clip. Applicable only to 1D double parameters. It is an
error to set this on any other type of double parameter.

For example a plugin that stabalises all the images in a clip to a
specific frame would have a *reference frame* parameter and declare that
as an absolute time parameter and use its value to fetch a frame to
stablise against.

Spatial Parameters
------------------

Parameters that can represent a size or position are essential. To that
end there are several values of the
:c:macro:`kOfxParamPropDoubleType` that say it
should be interpreted as a size or position, in either one or two
dimensions.

The original OFX API only specified
:ref:`normalised <ParameterPropertiesDoubleTypesNormalised>` parameters,
this proved to be somewhat more of a problem than expected. With the 1.2
version of the API,
:ref:`spatial <ParameterPropertiesDoubleTypesStrictlySpatial>` parameters
were introduced. Ideally these should be used and the normalised
parameter types should be deprecated.

Plugins can check :c:macro:`kOfxPropAPIVersion` to see if
these new parameter types are supported, in hosts with version 1.2 or
greater they will be.

See the section on :ref:`coordinate systems <CoordinateSystems>` to
understand some of the terms being discussed.

.. _ParameterPropertiesDoubleTypesStrictlySpatial:

Spatial Double Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~

These parameter types represent a size or position in one or two
dimensions in :ref:`Canonical Coordinate <CanonicalCoordinates>`. The host
and plug-in get and set values in this coordinate system. Scaling to
:ref:`Pixel Coordinate <PixelCoordinates>` is the responsibility of the
effect.

The default value of a spatial parameter can be set in either a
normalised coordinate system or the canonical coordinate system. This is
controlled by the
:c:macro:`kOfxParamPropDefaultCoordinateSystem`
on the parameter descriptor with one of the following value:

.. doxygendefine:: kOfxParamCoordinatesCanonical

.. doxygendefine:: kOfxParamCoordinatesNormalised

Parameters can choose to be spatial in several ways...

-  :c:macro:`kOfxParamDoubleTypeX`
    size in the X dimension, in canonical coords (1D double only),
-  :c:macro:`kOfxParamDoubleTypeXAbsolute`
    positing in the X axis, in canonical coords (1D double only)
-  :c:macro:`kOfxParamDoubleTypeY`
    size in the Y dimension, in canonical coords (1D double only),
-  :c:macro:`kOfxParamDoubleTypeYAbsolute`
    positing in the Y axis, in canonical coords (1D double only)
-  :c:macro:`kOfxParamDoubleTypeXY`
    2D size, in canonical coords (2D double only),
-  :c:macro:`kOfxParamDoubleTypeXYAbsolute`
    2D position, in canonical coords. (2D double only).

.. _ParameterPropertiesDoubleTypesNormalised:

Spatial Normalised Double Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Ideally, normalised parameters should be deprecated and no longer used
if :ref:`spatial <ParameterPropertiesDoubleTypesSpatial>` parameters are
available.

There are several values of the
:c:macro:`kOfxParamPropDoubleType` that say it
should be interpreted as a size or position. These are expressed and
proportional to the current project's size. This will allow the
parameter to scale cleanly with project size changes and to be
represented to the user in an appropriate range.

For example, the sensible X range of a visual effect plugin is the
project's width, say 768 pixels for a PAL D1 definition video project.
The user sees the parameter as 0..768, the effect sees it as 0..1. So if
the plug-in wanted to set the default value of an effect to be the
centre of the image, it would flag a 2D parameter as normalised and set
the defaults to be 0.5. The user would see this in the centre of the
image, no matter the resolution of the project in question. The plugin
would retrieve the parameter as 0..1 and scale it up to the project size
to size to use.

Parameters can choose to be normalised in several ways...

-  :c:macro:`kOfxParamDoubleTypeNormalisedX`
    normalised size wrt to the project's X dimension (1D only),
-  :c:macro:`kOfxParamDoubleTypeNormalisedXAbsolute`
    normalised absolute position on the X axis (1D only)
-  :c:macro:`kOfxParamDoubleTypeNormalisedY`
    normalised size wrt to the project's Y dimension(1D only),
-  :c:macro:`kOfxParamDoubleTypeNormalisedYAbsolute`
    normalised absolute position on the Y axis (1D only)
-  :c:macro:`kOfxParamDoubleTypeNormalisedXY`
    normalised to the project's X and Y size (2D only),
-  :c:macro:`kOfxParamDoubleTypeNormalisedXYAbsolute`
    normalised to the projects X and Y size, and is an absolute
   position on the image plane.

See the section on :ref:`coordinate systems <CoordinateSystems>` on how to
scale between normalised, canonical and pixel coordinates.

.. ParameterPropertiesDoubleTypesDefaultsAndAll:

Double Parameters Defaults, Increments, Mins and Maxs
-----------------------------------------------------

In all cases double parameters' defaults, minimums and maximums are
specified in the same space as the parameter, as is the increment in all
cases but normalised parameters.

Normalised parameters specify their increments in canonical
coordinates, rather than in normalised coordinates. So an increment of
'1' means 1 pixel, not '1 project width', otherwise sliders would be a
bit wild.

.. ParametersParametric:

Parametric Parameters
---------------------

.. ParametersParametricIntro:

Introduction
^^^^^^^^^^^^

Parametric params are new for 1.2 and are optionally supported by host
applications. They are specified via the :c:macro:`kOfxParamTypeParametric`
identifier passed into
:cpp:func:`OfxParameterSuiteV1::paramDefine`

These parameters are somewhat more complex than normal parameters and
require their own set of functions to manage and manipulate them. The
new :cpp:class:`OfxParametricParameterSuiteV1`
is there to do that.

All the defines and suite definitions for parameteric parameters are
defined in the file `ofxParametricParam.h <https://github.com/ofxa/openfx/blob/master/include/ofxParametricParam.h>`_

Parametric parameters are in effect *functions* a plug-in can ask a host
to arbitrarily evaluate for some value *x*. A classic use case would be
for constructing look-up tables, a plug-in would ask the host to
evaluate one at multiple values from 0 to 1 and use that to fill an
array.

A host would probably represent this to a user as a cubic curve in a
standard curve editor interface, or possibly through scripting. The user
would then use this to define the 'shape' of the parameter.

The evaluation of such params is not the same as animation, they are
returning values based on some arbitrary argument orthogonal to time, so
to evaluate such a param, you need to pass a parametric position and
time.

Often, you would want such a parametric parameter to be
multi-dimensional, for example, a colour look-up table might want three
values, one for red, green and blue. Rather than declare three separate
parametric parameters, so a parametric parameter can be
multi-dimensional.

Due to the nature of the underlying data, you *cannot* call certain
functions in the ordinary parameter suite when manipulating a parametric
parameter. All functions in the standard parameter suite are valid when
called on a parametric parameter, with the exception of the
following....

-  :cpp:func:`OfxParameterSuiteV1::paramGetValue`
-  :cpp:func:`OfxParameterSuiteV1::paramGetValueAtTime`
-  :cpp:func:`OfxParameterSuiteV1::paramGetDerivative`
-  :cpp:func:`OfxParameterSuiteV1::paramGetIntegral`
-  :cpp:func:`OfxParameterSuiteV1::paramSetValue`
-  :cpp:func:`OfxParameterSuiteV1::paramSetValueAtTime`

Defining Parametric Parameters
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Parametric parameters are defined using the standard parameter suite
function
:cpp:func:`OfxParameterSuiteV1::paramDefine`.
The descriptor returned by this call have several non standard
parameter properties available. These are

-  :c:macro:`kOfxParamPropParametricDimension`
    the dimension of the parametric parameter,
-  :c:macro:`kOfxParamPropParametricUIColour`
    the colour of the curves of a parametric parameter in any user
   interface
-  :c:macro:`kOfxParamPropParametricInteractBackground`
    a pointer to an interact entry point, which will be used to draw a
   background under any user interface,
-  :c:macro:`kOfxParamPropParametricRange`
    the min and max value that the parameter will be evaluated over.

Animating Parametric Parameters
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Animation is an optional host feature for parametric parameters. Hosts
flag whether they support this feature by setting the host descriptor
property
:c:macro:`kOfxParamHostPropSupportsParametricAnimation`.

Getting and Setting Values on a Parametric Parameters
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Seeing as we need to pass in the parametric position and dimension to
evaluate, parametric parameters need a new evaluation mechanism. They do
this with the
:cpp:func:`OfxParametricParameterSuiteV1::parametricParamGetValue` function.
This function returns the value of the parameter at the given time, for
the given dimension, adt the given parametric position,.

Parametric parameters are effectively interfaces to some sort of host
based curve library. To get/set/delete points in the curve that
represents a parameter, the new suite has several functions available to
manipulate control points of the underlying curve.

To set the default value of a parametric parameter to anything but the
identity, you use the control point setting functions in the new suite
to set up a curve on the *descriptor* returned by
:cpp:func:`OfxParameterSuiteV1::paramDefine`.
Any instances later created, will have that curve as a default.

Example
^^^^^^^

This simple example defines a colour lookup table, defines a default,
and show how to evaluate the curve

    ::

        // describe our parameter in 
        static OfxStatus
        describeInContext( OfxImageEffectHandle  effect,  OfxPropertySetHandle inArgs)
        {
          ....
          // define it
          OfxPropertySetHandle props;
          gParamHost->paramDefine(paramSet, kOfxParamTypeParametric, "lookupTable", & props);

          // set standard names and labeles
          gPropHost->propSetString(props, kOfxParamPropHint, 0, "Colour lookup table");
          gPropHost->propSetString(props, kOfxParamPropScriptName, 0, "lookupTable");
          gPropHost->propSetString(props, kOfxPropLabel, 0, "Lookup Table");

          // define it as three dimensional
          gPropHost->propSetInt(props, kOfxParamPropParametricDimension, 0, 3);

          // label our dimensions are r/g/b
          gPropHost->propSetString(props, kOfxParamPropDimensionLabel, 0, "red");
          gPropHost->propSetString(props, kOfxParamPropDimensionLabel, 1, "green");
          gPropHost->propSetString(props, kOfxParamPropDimensionLabel, 2, "blue");

          // set the UI colour for each dimension
          for(int component = 0; component < 3; ++component) {
             gPropHost->propSetDouble(props, kOfxParamPropParametricUIColour, component * 3 + 0, component % 3 == 0 ? 1 : 0);
             gPropHost->propSetDouble(props, kOfxParamPropParametricUIColour, component * 3 + 1, component % 3 == 1 ? 1 : 0);
             gPropHost->propSetDouble(props, kOfxParamPropParametricUIColour, component * 3 + 2, component % 3 == 2 ? 1 : 0);
          }

          // set the min/max parametric range to 0..1
          gPropHost->propSetDouble(props, kOfxParamPropParametricRange, 0, 0.0);
          gPropHost->propSetDouble(props, kOfxParamPropParametricRange, 1, 1.0);
         
          // set a default curve, this example sets an invert 
          OfxParamHandle descriptor;
          gParamHost->paramGetHandle(paramSet, "lookupTable", &descriptor, NULL);
          for(int component = 0; component < 3; ++component) {
            // add a control point at 0, value is 1
            gParametricParamHost->parametricParamAddControlPoint(descriptor,
                                                                  component, // curve to set 
                                                                  0.0,   // time, ignored in this case, as we are not adding a ket
                                                                  0.0,   // parametric position, zero
                                                                  1.0,   // value to be, 1
                                                                  false);   // don't add a key
            // add a control point at 1, value is 0
            gParametricParamHost->parametricParamAddControlPoint(descriptor, component, 0.0, 1.0, 0.0, false);
          }
         
          ...
        }

        void render8Bits(double currentFrame, otherStuff...)
        {
           ...

           // make three luts from our curves   
           unsigned char lut[3][256];

          OfxParamHandle param;
          gParamHost->paramGetHandle(paramSet, "lookupTable", &param, NULL);
          for(int component = 0; component < 3; ++component) {
            for(int position = 0; position < 256; ++position) {
              // position to evaluate the param at
              float parametricPos = float(position)/255.0f;
               
              // evaluate the parametric param
              float value;
              gParametricParamHost->parametricParamGetValue(param, component, currentFrame, parametricPos, &value);
              value = value * 255;
              value = clamp(value, 0, 255);

              // set that in the lut
              lut[dimension][position] = (unsigned char)value;
            }
          }
          ...
        }

.. _SettingParams:

Setting Parameters
------------------

Plugins are free to set parameters in limited set of circumstances,
typically relating to user interaction. You can only set parameters in
the following actions passed to the plug-in's :ref:`main entry <mainEntry>`
function...

-  :c:macro:`kOfxActionCreateInstance`
-  :c:macro:`kOfxActionBeginInstanceChanged`
-  :c:macro:`kOfxActionInstanceChanged`
-  :c:macro:`kOfxActionEndInstanceChanged`
-  :c:macro:`kOfxActionSyncPrivateData`

Plugins can also set parameter values during the following actions
passed to any of its interacts main entry function:

-  :c:macro:`kOfxInteractActionPenDown`
-  :c:macro:`kOfxInteractActionPenMotion`
-  :c:macro:`kOfxInteractActionPenUp`
-  :c:macro:`kOfxInteractActionKeyDown`
-  :c:macro:`kOfxInteractActionKeyRepeat`
-  :c:macro:`kOfxInteractActionKeyUp`
-  :c:macro:`kOfxInteractActionLoseFocus`
