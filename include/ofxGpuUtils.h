#ifndef _ofxGpuUtils_h_
#define _ofxGpuUtils_h_

/*
Software License :

Copyright (c) 2003-2019, The Open Effects Association Ltd. All rights reserved.

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

/** brief GpuUtilsPropCPUEffectUsesGPU  to reguster a CPU-based effect is using GPU ressources
	
	- Type - int X 1
	- Property Set - plugin only (write), plugin can change in Instance Changed action.
	- Valid Values - This must be one of :
	0: Do not use GPU
	1: Uses GPU

	Default: 0

	Hint to host that Effect request CPU images but still use GPU. 
	With the lack of GPU API standards (or the amount of them), it's likely an host and plug-in don't support the same APIs.
	This is of course ignored by host when plug-in uses a GPU suite like the ofxOpenGL suite
*/
#define kOfxGpuUtilsPropCPUEffectUsesGPU "OfxGpuUtilsPropCPUEffectUsesGPU"

/** brief GpuUtilsPropCPUEffectUsesGPU  to reguster a CPU-based effect is using GPU ressources
	- Type - int X 1
	- Property Set - plugin only (write), plugin can change in Instance Changed action.
	- Valid Values - This must be one of :
	0: Not same result
	1: Same result
	Default: 0

	If effect says YES, then host can fallback to CPU without expected issues.
	If effect says NO, host cannot fallback to CPU at least not without deleting the whole sequence from its cache maybe.
	Same result is a choice made by effect (then cache free because of a parameter change)
*/
#define kOfxGpuUtilsEffectProducesSameResultCPUorGPU "OfxGpuUtilEffectProducesSameResultCPUorGPU"
 
/** brief GpuUtilsHostTracksMemoryUsage  to register GPU memory management
	- Type - int X 1
	- Property Set - plugin only (read-write) - wr, host
	- Valid Values - This must be one of :
	0: Host does not respond
	1: Host tracks GPU memory usage
	Default: 0

	If hosts tracks GPU memory and plugin does not set this YES then Host can ignore GPU render if it wants
	If host says yes, the effect can return an out of memory error and expect the host to retry.
	see ofxGpuUtilsEffectMemoryNeeded
*/
#define kOfxGpuUtilsSupportsAllGPUNotTheSameModel "OfxGpuUtilsSupportsAllGPUNotTheSameModel"

/** brief GpuUtilsHSupportsAllGPUNotTheSameBrandModel    
	- Type - int X 1
	- Property Set - plugin only (read-write) 
	- Valid Values - This must be one of :
	0: Effect cannot guarantee same result on different GPU models
	1: Effect produces same result on different GPU models
	Default: 0

	Say you have a new and an old AMD GPU, can they be used on same sequence, 
	This is analogue kOfxGpuUtilsEffectProducesSameResultCPUorGPU
	Many hosts don't support different GPU at once so property likely to be ignored
*/
#define kOfxGpuUtilsSupportsAllGPUNotTheSameBrandModel "OfxGpuSupportsAllGPUNotTheSameBrandModel"
 
/** brief GpuUtilsSlaveToHostSettings
	- Type - int X 1
	- Property Set - plugin only(read - write) - write only in Instance Creation and Changed,  host
	- Valid Values - This must be one of :
	0 : Effect does slaves to host settings
	1 : Effect slaves to host settings
	Default : 0

	This is for host that offer option as global preferences in UI to use GPU or not to render
	For example an effect might have a GPU menu in each plugin, then a menu option saying Slave to Host Preferences could be added.
 */
#define kOfxGpuUtilsSlaveToHostSettings "OfxGpuUtilsSlaveToHostSettings"

/** brief GpuUtilsMemoryManagementHintCache in Bytes
	- Type - int X 1
	- Property Set - plugin only(write) - write only in Instance Creation and Changed,  host
	- Valid Values - Up to max VRAM available?
	Default : 0  // effect has not set, host does not support?

	Purpose is to request a GPU allocation to be retained until host needs space for something else... 
	This is likely for contexts where a user spends a lot of time on a frame...
	TO RESOLVE:  How is this implemented? By creating an Instance Changed?

*/
#define kOfxGpuUtilsMemoryManagementHintCache "OfxGpuUtilsMemoryManagementHintCache"

/** brief GpuUtilsMemoryManagementMemoryNeeded in Bytes
	- Type - int X 1
	- Property Set - plugin only(read - write) must request before render actions
	- Valid Values - Up to max VRAM available? 
 	Default : 0  // effect has not set

	UNRESOLVED: Implies a mechanism to retry defined providing additional memory amount needed(recursive until effect can run or host is out of available VRAM)

*/

/** @brief render ran out of memory, note same error number as kOfxStatGLOutOfMemory */
#define kOfxGpuUtilsStatOutOfMemory  ((int) 1001)

/** brief GpuUtilsMemoryManagementMemoryNeeded in Bytes
	- Type - int X 1
	- Property Set - plugin only(read - write) must request before render actions
	- Valid Values - Up to max VRAM available?
	Default : 0  // effect has not set

	UNRESOLVED: Implies a mechanism to retry defined providing additional memory amount needed (recursive until effect can run or host is out of available VRAM)
	Is this  kOfxGpuUtilsStatOutOfMemory?

*/
#define kOfxGpuUtilsMemoryManagementMemoryNeeded "OfxGpuUtilsMemoryManagementMemoryNeeded"


/** brief GpuUtilsMemoryManagementMemoryNeeded in Bytes
- Type - int X 1
- Property Set - host
- Valid Values - Up to max GPU in system
Default : 0  // effect has not set ?

	UNRESOLVED: Implies a universal GPU enumeration for a particular machine independent of driver, API.
	In practice as host might be internally running whatever OpenGL, Cuda, OpenCL, Dx, Metal, Vulkan,... 
	For example I don't think CUDA would assign a GPUid to Integrated Graphucs.
*/
#define kOfxGpuUtilsRunOnGpuIndex "OfxGpuUtilsRunOnGpuIndex"

/**  Open Question : Switching from CPU to GPU or reverse might affect plugin ThreadSafety reported by plugin?  */



#ifdef __cplusplus
}
#endif


/** @file ofxGpuUtils.h

This header contains the suite definition to manipulate host side parameters.

For more details go see @ref GpuUtils


#endif