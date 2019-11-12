Changes to the API for 1.2
==========================

Introduction
------------

This chapter lists the changes and extensions between the 1.1 version of
the API and the 1.2 version of the API. The extension are backwards
compatible, so that a 1.2 plugin will run on an earlier version of a
host, provided a bit of care is taken. A 1.2 host will easily support a
plugin written to an earlier API.


Packaging
---------

A new architecture directory was added to the bundle hierarchy to
specifically contain Mac OSX 64 bit builds. The current 'MacOS'
architecture is a fall back for 32 bit only and/or universal binary
builds.


Versioning
----------

Three new properties are provided to identify and version a plugin
and/or host. These are...

-  :c:macro:`kOfxPropAPIVersion` a multi-dimensional integer that specifies the version of the API
   being implemented by a host.
-  :c:macro:`kOfxPropVersion` a multi-dimensional integer that provides a version number for host
   and plugin
-  :c:macro:`kOfxPropVersionLabel` a user readable version label

Before 1.2 there was no way to identify the version of a host
application, which a plugin could use to work around known bugs and
problems in known versions. :c:macro:`kOfxPropVersion`
provides a way to do that.


Plugin Description
------------------

The new property :c:macro:`kOfxPropPluginDescription`
allows a plugin to set a string which provides a description to a user.

Parameter Groups and Icons
--------------------------

Group parameters are typically displayed in a hierarchical manner on
many applications, with 'twirlies' to open and close the group. The new
property :c:macro:`kOfxParamPropGroupOpen` is used to
specify if a group parameter should be initially open or closed.

Some applications are able to display icons instead of text when
labelling parameters. The new property,
:c:macro:`kOfxPropIcon`, specifies an SVG file and/or a PNG
file to use as an icon in such host applications.

New Message Suite
-----------------

A new message suite has been specified,
:cpp:class:`OfxMessageSuiteV2`, this adds two new functions.
One to set a persistent message on an effect, and a second to clear that
message. This would typically be used to flag an error on an effects.

New Syncing Property
---------------------

A new property has been added to parameter sets,
:c:macro:`kOfxPropParamSetNeedsSyncing`. This
is used by plugins with internal data structures that need syncing back
to parameters for persistence and so on. This property should be set
whenever the plugin changes it's internal state to inform the host that
a sync will be required before the next serialisation of the plugin.
Without this property, the host would have to continually force the
plugin to sync it's private data, whether that was a redundant operation
or not. For large data sets, this can be a significant overhead.


Sequential Rendering
---------------------

Flagging sequential rendering has been slightly modified. The
:c:macro:`kOfxImageEffectInstancePropSequentialRender`
property has had a third allowed state added, which indicate that a
plugin would prefer to be sequentially rendered if possible, but need
not be.

The :c:macro:`kOfxImageEffectInstancePropSequentialRender`
property has also been added to the host descriptor, to indicate whether
the host can support sequential rendering.

The new property :c:macro:`kOfxImageEffectPropSequentialRenderStatus`
is now passed to the render actions to indicate that a host is currently
sequentially rendering or not.

Interactive Render Notification
-------------------------------

A new property has been added to flag a render as being in response to
an interactive change by a user, as opposed to a batch render. This is
:c:macro:`kOfxImageEffectPropInteractiveRenderStatus`


Host Operating System Handle
-----------------------------

A new property has been added to allow a plugin to request the host
operating system specific application handle (ie: on Windows (tm) this
would be the application's root hWnd). This is
:c:macro:`kOfxPropHostOSHandle`


Non Normalised Spatial Parameters
---------------------------------

Normalised double parameters have proved to be more of a problem than
expected. The major idea was to provide resolution independence for
spatial parameters. However, in practice, having to specify parameters
as a fraction of a yet to be determined resolution is problematic. For
example, if you want to set something to be explicitly '20', there is no
way of doing that. The main problem stems from normalised params
conflating two separate issues, flagging to the host that a parameter
was spatial, and being able to specify defaults in a normalised
co-ordinate system.

With 1.2 new :ref:`spatial double
parameter <ParameterPropertiesDoubleTypesSpatial>` types are defined.
These have their values manipulated in canonical coordinates, however,
they have an option to specify their default values in a normalise
coordinate system. These are....

These new double parameter types are....

-  :c:macro:`kOfxParamDoubleTypeX`
   - a size in the X dimension dimension (1D only), new for 1.2
-  :c:macro:`kOfxParamDoubleTypeXAbsolute`
   - a position in the X dimension (1D only), new for 1.2
-  :c:macro:`kOfxParamDoubleTypeY`
   - a size in the Y dimension dimension (1D only), new for 1.2
-  :c:macro:`kOfxParamDoubleTypeYAbsolute`
   - a position in the X dimension (1D only), new for 1.2
-  :c:macro:`kOfxParamDoubleTypeXY`
   - a size in the X and Y dimension (2D only), new for 1.2
-  :c:macro:`kOfxParamDoubleTypeXYAbsolute`
   - a position in the X and Y dimension (2D only), new for 1.2

These new parameter types can set their defaults in one of two
coordinate systems, the property
:c:macro:`kOfxParamPropDefaultCoordinateSystem`
Specifies the coordinate system the default value is being specified in.

Plugins can check :c:macro:`kOfxPropAPIVersion` to see if
these new parameter types are supported

.. APIChanges_1_2_Native_Overlay_Handles:

Native Overlay Handles
----------------------

Some applications have their own overlay handles for certain types of
parameter (eg: spatial positions). It is often better to rely on those,
than have a plugin implement their own overlay handles. Two new
parameter, properties are available to do that, one used by the host to
indicate if such handles are available. The other by a plugin telling
the host to use such handle.

-  :c:macro:`kOfxParamPropHasHostOverlayHandle`
   indicates a parameter has an host native overlay handle
-  :c:macro:`kOfxParamPropUseHostOverlayHandle`
   indicates that a host should use a native overlay handle.

Interact Colour Hint
--------------------

Some applications allow the user to specify colours of any overlay via a
colour picker. Plug-ins can access this via the
:c:macro:`kOfxInteractPropSuggestedColour`
property.


Interact Viewport Pen Position
-------------------------------

The new property
:c:macro:`kOfxInteractPropPenViewportPosition`
is used to pass a pen position in viewport coordinate, rather than a
connaonical. This is sometimes much more convenient. It is passed to all
actions that
:c:macro:`kOfxInteractPropPenPosition` is passed to.


Parametric Parameters
---------------------

A new optional parameter type, and supporting suite, is introduced,
parametric parameters.
This allows for the construction of user defined lookup tables and so
on.
