#ifndef _ofxColourSpace_h_
#define _ofxColourSpace_h_

/*
Software License :

Copyright (c) 2003-2020, The Open Effects Association Ltd. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
* Neither the name The Open Effects Association Ltd, nor the names of its
contributors may be used to endorse or promote products derived from this
software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


 

#ifdef __cplusplus
extern "C" {
#endif

/** @brief OfxImageEffectPropColourSpaceType:  Purpose is to query ColourSpace and basically return convert or not 
	- Type - string X 1
	- Valid Values - This must be one of : (rw)
		- kOfxColorSceneReferredLinear
	    - kOfxColorGammaEncoded     // sRGB, rec 709, gamma 2.2 CGI, rec 601, rec 2020 etc any sort of gamma encoded type, typically 1.9 to 2.6
		- kOfxColorLog              // Cineon, S-Log, ACEScc - this includes also hybrid camera space like log-gamma
		- kOfxColorUnmanaged              //  unmanaged -- no transform because no known transform

	Plugin understands that this does not contain chromacity information and tone is essentially defined as linear, gamma or log - without defining actual curve.
*/

#define kOfxImageEffectPropColourSpaceType       "OfxImageEffectPropColourSpaceType"

/** @brief kOfxImageEffectSupportsMultipleColourSpaces:  Specify whether different clips can request different ColourSpaces
	- Type - int X 1
	- Property Set - plugin descriptor (read/write), host descriptor (read only)
	- Default - 0 for a plugin
	- Valid Values - This must be one of
     - 0 - in which case the host or plugin does not support multiple colour spaces,
     - 1 - which means a host or plugin is able to to deal with multiple colour spaces,

	This is similar to kOfxImageEffectPropSupportsMultiResolution, kOfxImageEffectPropSupportsMultipleClipDepths, kOfxImageEffectPropSupportsMultipleClipPARs...
*/

#define kOfxImageEffectSupportsMultipleColourSpaces "OfxImageEffectSupportsMultipleColourSpaces"


/** @brief OfxImageEffectPropColorHostManaged:  Purpose is to indicate to host to go ahead and ColorManage
	- Type - int X 1
	- Valid Values - This must be one of :
		0: Do not Color Manage
		1: Color Manage - means host can color manage (see SupportedColourSpaces) 

	Default: 0

	If both true (host and plugin), then the net effect here is it then bypasses  ColourSpaceType (it's either one or other) 
	Plugin can still query ColourSpaceType but that becomes effectively read-only property if ColorHostManaged is set to true.
*/

#define kOfxImageEffectPropColorHostManaged       "OfxImageEffectPropColorHostManaged"

// Other defined ImageColourSpace can be added, adding and not supporting a ColourSpace is non-destructive
#define kOfxColourSpace_unknown_linear	"OfxColourSpace_unknown_linear" // unknown primaries, linearised - this is the default in Baselight
#define kOfxColourSpace_unknown_unknown "OfxColourSpace__unknown_unknown" // - no conversion at all, no information at all
#define kOfxColourSpace_rec709_linear  "OfxColourSpace__rec709_linear" // - Rec.709 primaries, linear tone curve
#define kOfxColourSpace_aces_linear  "OfxColourSpace_aces_linear" // - ACES primaries, linear tone curve
#define kOfxColourSpace_arriwg_arrilogc "OfxColourSpace_arriwg_arrilogc" // - ARRI WideGamut primaries, ARRI LogC tone curve
#define kOfxColourSpace_sonysgamut3cine_sonyslog3 "OfxColourSpace_sonysgamut3cine_sonyslog3" // - Sony SGamut3.cine primaries, Sony SLog3 tone curve
#define kOfxColourSpace_rec709_rec1886 "OfxColourSpace_rec709_rec1886"  // Rec.709 primaries, Rec.1886 tone curve(display - referred)

/** brief: Set list of colour space supported
	- Type - string X N
	- Valid Values: An enumerated kOfxColourSpace_ string

	These are two lists of strings of kOfxImageColourSpace, what the plugin supports and what the host supports
	Host will treat first entry of plugin reported supported string if the plugin does not support the host currently defined ColourSpace
*/
#define kOfxImageEffectPropSupportedColourSpaces  "OfxImageEffectPropSupportedColourSpaces"

/**  brief: Returns colourspace of effect
	- Type - string X 1
	- Valid Values: An enumerated kOfxColourSpace_ string

	During OfxImageEffectActionRender, the host sets the "OfxImageEffectPropColourSpace" (string) property to indicate the actual colour space used. 
	Without doing it on a per clip basis, this transform will be applied to all clip.

*/
#define kOfxImageEffectPropColourSpace "OfxImageEffectPropColourSpace"

/** brief: Returns colourspace of clip
	- Type - string X 1
	- Valid Values: An enumerated kOfxColourSpace_ string


	During OfxImageEffectActionRender, the clip sets the "OfxImageEffectPropColourSpace" (string) property to indicate the actual colour space it wants the image in. 
	kOfxImageEffectSupportsMultipleColourSpaces must be true for per clip Colour Space to be requested
*/

#define kOfxImageClipPropColourSpace "OfxImageClipPropColourSpace"

/** @brief The approximate gamma value

- Type - double X 1
- Default: 1.0  - linear
- Property:  Read-Only
- Valid Values - can be any positive value, usual suspects are 1.0, 2.2, 2.4, 2.6

	This property is slightly orthogonal to the other properties in this set. The Host only provides the info. 
	The main purpose is for a plug-in to internally linearize and delinearize non-destructively in a less arbitrary way.
	This does not change what the plug-in gets or what the plugin returns to the host.
	An host might have at project level a UI switch allowing users to work linear or color managed. This reduce the chances that such thing breaks a user setup.
	A plugin might have a library that is assuming a gamma value (even if 1.0), this provides a way for plugin to linearize or delinearize based on information provided by host.
	
*/
#define kOfxImageEffectPropColourSpaceApproximateGamma "kOfxImageEffectPropColourApproximateGamma"


ifdef __cplusplus
}
#endif

 

#endif
