.. _saturationExample:

This guide will take you through the basics of creating effects that can
be used in more than one context, as well as how to make a multi-input
effect. Its source can be found in the C++ file
`saturation.cpp <https://github.com/ofxa/openfx/blob/master/Guide/Code/Example4/saturation.cpp>`_.
This plugin takes an RGB or RGBA
image and increases or descreases the saturation by a parameter. It can
be used in two contexts, firstly as a simple filter, secondly as a
general effect, where it has an optional second input clip which is used
to control where the effect is applied.

.. _multiple_contexts_why_bother:

Multiple Contexts, Why Bother?
==============================

As briefly described in the first example, OFX has the concept of
contexts that an effect can be used in. Our example is going to work in
the filter context and the general context.

The rules for a filter context are that it has to have one and only one
input clip, called *Source* and one and only one output clip called
*Output*.

For a general context, you have to have a single mandated clip called
*Output* and that is it. You are free to have as many input clips as you
need, name them how you feel and use choose how to set certain important
properties of the output.

Why would we want to do this? Because not all host applications behave
the same way. For example an editing application will typically allow
effects to be applied to clips on a timeline, and the effect can only
take a single input when used like that. A complicated node-based
compositor is less restrictive, its effect can typically have any number
of inputs and the rules for certain behaviours are relaxed.

So you’ve written your OFX effect, and it can work with a single input,
but would ideally work much better with multiple inputs. You also want
it to work as best it can across a range of host applications. If you
could only write it as a multi-input general effect with more than one
input, it couldn’t work in an editor. However if you wrote it as a
single input effect, it wouldn’t work as well as it could in a node
based compositor. Having your effect work in multiple contexts is the
way to have it work as best as possible in both applications.

In this way an OFX host application, which knows which contexts it can
support, will inspect the contexts a plugin says it can be used it, and
choose the most appropriate one for what it wants to do.

This example plugin shows you how to do that.

.. _describing_our_plugin:

Describing Our Plugin
=====================

Our basic describe action is pretty much the same as all the other
examples, but with one minor difference, we set two contexts in which
the effect can be used in.

`saturation.cpp <https://github.com/ofxa/openfx/blob/doc/Documentation/sources/Guide/Code/Example4/saturation.cpp#347>`_

::

        // Define the image effects contexts we can be used in, in this case a filter
        // and a general effect.
        gPropertySuite->propSetString(effectProps,
                                      kOfxImageEffectPropSupportedContexts,
                                      0,
                                      kOfxImageEffectContextFilter);

        gPropertySuite->propSetString(effectProps,
                                      kOfxImageEffectPropSupportedContexts,
                                      1,
                                      kOfxImageEffectContextGeneral);

The snippet above shows that the effect is saying it can be used in the
filter and general contexts.

Both of these have rules associated as to how the plugin behaves in that
context. Because the filter context is so simple, most of the default
behaviour just works and you don’t have to trap many other actions.

In the case of the general context, the default behaviour might not work
the way you want, and you may have to trap other actions. Fortunately
the defaults work for us as will.

`saturation.cpp <https://github.com/ofxa/openfx/blob/doc/Documentation/sources/Guide/Code/Example4/saturation.cpp#388>`_

.. code:: c++

      ////////////////////////////////////////////////////////////////////////////////
      //  describe the plugin in context
      OfxStatus
      DescribeInContextAction(OfxImageEffectHandle descriptor,
                              OfxPropertySetHandle inArgs)
      {
        // get the context we are being described for
        char *context;
        gPropertySuite->propGetString(inArgs, kOfxImageEffectPropContext, 0, &context);

        OfxPropertySetHandle props;
        // define the mandated single output clip
        gImageEffectSuite->clipDefine(descriptor, "Output", &props);

        // set the component types we can handle on out output
        gPropertySuite->propSetString(props,
                                      kOfxImageEffectPropSupportedComponents,
                                      0,
                                      kOfxImageComponentRGBA);
        gPropertySuite->propSetString(props,
                                      kOfxImageEffectPropSupportedComponents,
                                      1,
                                      kOfxImageComponentRGB);

        // define the mandated single source clip
        gImageEffectSuite->clipDefine(descriptor, "Source", &props);

        // set the component types we can handle on our main input
        gPropertySuite->propSetString(props,
                                      kOfxImageEffectPropSupportedComponents,
                                      0,
                                      kOfxImageComponentRGBA);
        gPropertySuite->propSetString(props,
                                      kOfxImageEffectPropSupportedComponents,
                                      1,
                                      kOfxImageComponentRGB);

        if(strcmp(context, kOfxImageEffectContextGeneral) == 0) {
          gImageEffectSuite->clipDefine(descriptor, "Mask", &props);

          // set the component types we can handle on our main input
          gPropertySuite->propSetString(props,
                                        kOfxImageEffectPropSupportedComponents,
                                        0,
                                        kOfxImageComponentAlpha);
          gPropertySuite->propSetInt(props,
                                     kOfxImageClipPropOptional,
                                     0,
                                     1);
          gPropertySuite->propSetInt(props,
                                     kOfxImageClipPropIsMask,
                                     0,
                                     1);
        }

        ...
        [SNIP]
        ...

        return kOfxStatOK;
      }

I’ve snipped the simple parameter definition code out to save some
space.

Here we have the describe in context action. This will now be called
once for each context that a host application wants to support. You know
which contex you are being described in by the
:c:macro:`kOfxImageEffectPropContext` property on inArgs.

Regardless of the context, it describes two clips, "Source" and
"Output", which will work fine both as a filter and a general context.
Note that we won’t support *alpha* on these two clips, we only support
images that have colour components, as how can you saturate a single
channel image?

Finally, if the effect is in the general context, we describe a third
clip and call it "Mask". We then tell the host about that clip…

-  firstly, that we only want single component images from that clip

-  secondly, that the clip is optional,

-  thirdly, that this clip is to be interpreted as a mask, so hosts
   that manage such things separately, know it can be fed into this
   input.

.. figure:: Pics/SaturationNuke.jpg
    :scale: 100 %
    :alt: Saturation Example in Nuke
    :align: center

The image above shows our saturation example running inside Nuke. Nuke
chose to instantiate the plugin as a general context effect, not a
filter, as general contexts are the ones it prefers. You can see the
graph, and our saturation node has two inputs, one for the mask and one
for the source image. The control panel for the effect is also shown,
with the saturation value set to zero. Note the extra *MaskChannel*
param, which was not specified by the plugin. This was automatically
generated by Nuke when it saw that the *Mask* input to the effect was a
single channel, so as to allow the user to choose which one to use as a
mask.

The result is an image whose desaturation amount is modulated by the
alpha channel of the mask image, which in this case is a right to left
ramp.

.. _the_other_actions:

The Other Actions
=================

All the other actions should be fairly familiar and you should be able
to reason them out pretty easily. The two that have any significant
differences because of the multi context use are the create instance
action and the render action.

.. _create_instance:

Create Instance
---------------

This is pretty familiar, though we have a slight change to handle the
mask input.

`saturation.cpp <https://github.com/ofxa/openfx/blob/doc/Documentation/sources/Guide/Code/Example4/saturation.cpp#483>`_

.. code:: c++

      ////////////////////////////////////////////////////////////////////////////////
      /// instance construction
      OfxStatus CreateInstanceAction( OfxImageEffectHandle instance)
      {
        OfxPropertySetHandle effectProps;
        gImageEffectSuite->getPropertySet(instance, &effectProps);

        // To avoid continual lookup, put our handles into our instance
        // data, those handles are guaranteed to be valid for the duration
        // of the instance.
        MyInstanceData *myData = new MyInstanceData;

        // Set my private instance data
        gPropertySuite->propSetPointer(effectProps, kOfxPropInstanceData, 0, (void *) myData);

        // is this instance made for the general context?
        char *context = 0;
        gPropertySuite->propGetString(effectProps, kOfxImageEffectPropContext, 0,  &context);
        myData->isGeneralContext = context &&
                                   (strcmp(context, kOfxImageEffectContextGeneral) == 0);

        // Cache the source and output clip handles
        gImageEffectSuite->clipGetHandle(instance, "Source", &myData->sourceClip, 0);
        gImageEffectSuite->clipGetHandle(instance, "Output", &myData->outputClip, 0);

        if(myData->isGeneralContext) {
          gImageEffectSuite->clipGetHandle(instance, "Mask", &myData->maskClip, 0);
        }

        // Cache away the param handles
        OfxParamSetHandle paramSet;
        gImageEffectSuite->getParamSet(instance, &paramSet);
        gParameterSuite->paramGetHandle(paramSet,
                                        SATURATION_PARAM_NAME,
                                        &myData->saturationParam,
                                        0);

        return kOfxStatOK;
      }

We are again using instance data to cache away a set of handles to clips
and params (the constructor of which sets them all to NULL). We are also
recording which context we have had our instance created for by checking
the :c:macro:`kOfxImageEffectPropContext` property of the effect. If it is a
general context we also cache the *Mask* input in our instance data.
Pretty easy.

.. _rendering:

Rendering
---------

Because we are now using a class to wrap up OFX images (see
:ref:`below <a_bit_of_houskeeping>`) the render code is a bit tidier but
is pretty much still the same really. The major difference is that we
are now fetching a third image, for the mask image, and we are prepared
for this to fail and keep going as we may be in the filter context, or
we may be in the general context but the clip is not connected.

`saturation.cpp <https://github.com/ofxa/openfx/blob/doc/Documentation/sources/Guide/Code/Example4/saturation.cpp#629>`_

.. code:: c++

      // Render an output image
      OfxStatus RenderAction( OfxImageEffectHandle instance,
                              OfxPropertySetHandle inArgs,
                              OfxPropertySetHandle outArgs)
      {
        // get the render window and the time from the inArgs
        OfxTime time;
        OfxRectI renderWindow;
        OfxStatus status = kOfxStatOK;

        gPropertySuite->propGetDouble(inArgs,
                                      kOfxPropTime,
                                      0,
                                      &time);
        gPropertySuite->propGetIntN(inArgs,
                                    kOfxImageEffectPropRenderWindow,
                                    4,
                                    &renderWindow.x1);

        // get our instance data which has out clip and param handles
        MyInstanceData *myData = FetchInstanceData(instance);

        // get our param values
        double saturation = 1.0;
        gParameterSuite->paramGetValueAtTime(myData->saturationParam, time, &saturation);

        // the property sets holding our images
        OfxPropertySetHandle outputImg = NULL, sourceImg = NULL, maskImg = NULL;
        try {
          // fetch image to render into from that clip
          Image outputImg(myData->outputClip, time);
          if(!outputImg) {
            throw " no output image!";
          }

          // fetch image to render into from that clip
          Image sourceImg(myData->sourceClip, time);
          if(!sourceImg) {
            throw " no source image!";
          }

          // fetch mask image at render time from that clip, it may not be there
          // as we might in the filter context or it might not be attached as it
          // is optional, so don't worry if we don't have one.
          Image maskImg(myData->maskClip, time);

          // now do our render depending on the data type
          if(outputImg.bytesPerComponent() == 1) {
            PixelProcessing<unsigned char, 255>(saturation,
                                                instance,
                                                sourceImg,
                                                maskImg,
                                                outputImg,
                                                renderWindow);
          }
          else if(outputImg.bytesPerComponent() == 2) {
            PixelProcessing<unsigned short, 65535>(saturation,
                                                   instance,
                                                   sourceImg,
                                                   maskImg,
                                                   outputImg,
                                                   renderWindow);
          }
          else if(outputImg.bytesPerComponent() == 4) {
            PixelProcessing<float, 1>(saturation,
                                      instance,
                                      sourceImg,
                                      maskImg,
                                      outputImg,
                                      renderWindow);
          }
          else {
            throw " bad data type!";
            throw 1;
          }

        }
        catch(const char *errStr ) {
          bool isAborting = gImageEffectSuite->abort(instance);

          // if we were interrupted, the failed fetch is fine, just return kOfxStatOK
          // otherwise, something weird happened
          if(!isAborting) {
            status = kOfxStatFailed;
          }
          ERROR_IF(!isAborting, " Rendering failed because %s", errStr);
        }

        // all was well
        return status;
      }

The actual pixel processing code does the standard saturation
calculation on each pixel, scaling each of R, G and B around their
common average. The tweak we add is to modulate the amount of the effect
by looking at the pixel values of the mask input if we have one. Again
this is not meant to be fast code, just illustrative.

`saturation.cpp <https://github.com/ofxa/openfx/blob/doc/Documentation/sources/Guide/Code/Example4/saturation.cpp#553>`_

.. code:: c++

      ////////////////////////////////////////////////////////////////////////////////
      // iterate over our pixels and process them
      template <class T, int MAX>
      void PixelProcessing(double saturation,
                           OfxImageEffectHandle instance,
                           Image &src,
                           Image &mask,
                           Image &output,
                           OfxRectI renderWindow)
      {
        int nComps = output.nComponents();

        // and do some processing
        for(int y = renderWindow.y1; y < renderWindow.y2; y++) {
          if(y % 20 == 0 && gImageEffectSuite->abort(instance)) break;

          // get the row start for the output image
          T *dstPix = output.pixelAddress<T>(renderWindow.x1, y);

          for(int x = renderWindow.x1; x < renderWindow.x2; x++) {

            // get the source pixel
            T *srcPix = src.pixelAddress<T>(x, y);

            // get the amount to mask by, no mask image means we do the full effect everywhere
            float maskAmount = 1.0f;
            if (mask) {
              // get our mask pixel address
              T *maskPix = mask.pixelAddress<T>(x, y);
              if(maskPix) {
                maskAmount = float(*maskPix)/float(MAX);
              }
              else {
                maskAmount = 0;
              }
            }

            if(srcPix) {
              if(maskAmount == 0) {
                // we have a mask input, but the mask is zero here,
                // so no effect happens, copy source to output
                for(int i = 0; i < nComps; ++i) {
                  *dstPix = *srcPix;
                  ++dstPix; ++srcPix;
                }
              }
              else {
                // we have a non zero mask or no mask at all

                // find the average of the R, G and B
                float average = (srcPix[0] + srcPix[1] + srcPix[2])/3.0f;

                // scale each component around that average
                for(int c = 0; c < 3; ++c) {
                  float value = (srcPix[c] - average) * saturation + average;
                  if(MAX != 1) {
                    value = Clamp<T, MAX>(value);
                  }
                  // use the mask to control how much original we should have
                  dstPix[c] = Blend(srcPix[c], value, maskAmount);
                }

                if(nComps == 4) { // if we have an alpha, just copy it
                  dstPix[3] = srcPix[3];
                }
                dstPix += 4;
              }
            }
            else {
              // we don't have a pixel in the source image, set output to zero
              for(int i = 0; i < nComps; ++i) {
                *dstPix = 0;
                ++dstPix;
              }
            }
          }
        }
      }

.. _a_bit_of_houskeeping:

A Bit Of Housekeeping
====================

You may have noticed I’ve gone and created an ``Image`` class. I got
bored of passing around various pointers and bounds and strides in my
code and decided to tidy it up.

`saturation.cpp <https://github.com/ofxa/openfx/blob/doc/Documentation/sources/Guide/Code/Example4/saturation.cpp#110>`_

.. code:: c++

      ////////////////////////////////////////////////////////////////////////////////
      // class to manage OFX images
      class Image {
      public    :
        // construct from a property set that represents the image
        Image(OfxPropertySetHandle propSet);

        // construct from a clip by fetching an image at the given frame
        Image(OfxImageClipHandle clip, double frame);

        // destructor
        ~Image();

        // get a pixel address, cast to the right type
        template <class T>
        T *pixelAddress(int x, int y)
        {
          return reinterpret_cast<T *>(rawAddress(x, y));
        }

        // Is this image empty?
        operator bool()
        {
          return propSet_ != NULL && dataPtr_ != NULL;
        }

        // bytes per component, 1, 2 or 4 for byte, short and float images
        int bytesPerComponent() const { return bytesPerComponent_; }

        // number of components
        int nComponents() const { return nComponents_; }

      protected :
        void construct();

        // Look up a pixel address in the image. returns null if the pixel was not
        // in the bounds of the image
        void *rawAddress(int x, int y);

        OfxPropertySetHandle propSet_;
        int rowBytes_;
        OfxRectI bounds_;
        char *dataPtr_;
        int nComponents_;
        int bytesPerComponent_;
        int bytesPerPixel_;
      };

It takes an :c:type:`OfxPropertySetHandle` and pulls all the bits it needs out of
that into a class. It uses all the same pixel access logic as in example
2. Ideally I should put this in a library which our example links to,
but I’m keeping all the code for each example in one source file for
illustrative purposes. Feel free to steal this and use it in your own
code  [1]_.

.. __summary:

Summary
=======

This plugin has shown you - the basics of working with multiple
contexts, - how to handle optional input clips, - restricting pixel
types on input and output clips.

.. [1]
   provided you stick to the conditions listed at the top of source file

