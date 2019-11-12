Hi,

the examples enclosed in this directory are designed to show certain features
of the OFX Image Effect API, they are not meant to be great examples of the
art of image processing or programming.

The current examples were written on and will compile on IRIX 6.5. They should
compile on other operating systems, though the Makefiles may not. As the API 
settles down, these examples will be compiled on more systems.

The current examples are all written in C++ and are...

     - Basic   - shows a simple luminance scaling plugin, which defines several
                 parameters, processes images and has actions to
	         respond to user interaction.
     - Overlay - shows how to draw a simple overlay widget using the OFX Interact
                 API.
     - Custom  - shows how custom parameters work within the API, it also uses the
	         OFX Interact API.
     - DepthConverter - shows how to deal with pixel preferences.
     - Rectangle - example generator and filter that shows how to deal with spatial
                 parameters.

The directory include contains the file ofxUtilities.H which contains some code 
shared by all the examples.

cheers

Bruno Nicoletti
