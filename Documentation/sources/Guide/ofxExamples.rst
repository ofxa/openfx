Foreword
========

OFX is an open API for writing visual effects plug-ins for a wide
variety of applications, such as video editing systems and compositing
systems. This guide demonstrates by example the low level C APIs that
defines OFX.


Intended Audience
=================

Do you write visual effects or image processing software? Do you have an
application which deals with moving images and hosts plug-ins or would
like to host plug-ins? Then OFX is for you.

This guide assumes you can program in the ``C`` language and are
familiar with the concepts involved in writing visual effects software.
You need to understand concepts like pixels, clips, pixel aspect ratios
and so on. If you don’t, I suggest you read further or attempt to
soldier on bravely and see how far you go before you get lost.


What is OFX?
============

OFX is actually several things. At the lowest level OFX is a generic
``C`` based plug-in architecture that can be used to define any kind of
plug-in API. You could use this low level architecture to implement any
API, however it was originally designed to host our visual effects image
processing API. The basic architecture could be re-used to create other
higher level APIs such as a sound effects API, a 3D API and more.

This guide describes the basic OFX plug-in architecture and the visual
effects plug-in API built on top of it. The visual effects API is very
broad and intended to allow visual effects plug-ins to work on a wide
range of host applications, including compositing hosts, rotoscopers,
encoding applications, colour grading hosts, editing hosts and more I
haven’t thought of yet. While all these type of applications process
images, they often have very different work flows and present effects to
a user in incompatible ways. OFX is an attempt to deal with all of these
in a clear and consistent manner.

The API is by design feature rich, not all aspects of the API map to all
hosts. This is to allow different host developers to implement OFX
support in a manner that best fits their applications' capabilities.

Hosts are encouraged to extend OFX by providing extra proprietary
suites, actions, properties and settings to extend the capabilities of
the API. It would be nice that a broadly useful proprietary extension be
put forward for incorporation into the open standard.

That said although there is no validation process in terms of what is an
OFX host, a small minimal set of expectations is assumed, which we will
cover in the following guides.

The Examples
============

I’ll illustrate the API and how it works with a variety of example
plugins, each of which will have it’s own guide describing what is going
one. You should work through them one by one as each will build on the
one before.

For completeness and clarity of explanation, each plugin is entirely
self contained and has no dependency on anything other than standard C
and C++ libraries and the OFX headers.

-  :ref:`The basic machinery of an OFX plugin. <basicExample>`

-  :ref:`How to access images. <invertExample>`

-  :ref:`How to define parameters. <gainExample>`

-  :ref:`How to write multi context effects. <saturationExample>`

-  :ref:`Coordinate systems and defining regions of
   definition. <circleExample>`


Wrapping the API
================

This API can be somewhat awkward to use directly, and it is expected
that most plugin or host developers will wrap the API in higher level C
or C++ structures.

There are open source host and plugin side API C wrappers available from the `openfx.org git repository <https://github.com/ofxa/openfx>`_.
As you work through the examples you’ll see that I actually start wrapping up various entities within the API
into ``C`` classes as it can get unwieldy otherwise.

License
=======

Please feel free to use any of the code you find here, provided you
adhere to the BSD style license you’ll find at the top of each header
file.
