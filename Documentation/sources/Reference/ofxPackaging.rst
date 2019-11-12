Packaging OFX Plug-ins
======================

Where a host application chooses to search for OFX plug ins, what binary
format they are in and any directory hierarchy is entirely up to it.
However, it is strongly recommended that the following scheme be
followed.

Binary Types
------------

Plug-ins should be distributed in the following formats, depending on
the host operating system....

-  Microsoft Windows, as ".dll" dynamically linked libraries,
-  Apple OSX, as Mach-O binary bundles,
-  IRIX and LINUX, as ELF dynamic shared objects.

Installation Directory Hierarchy
--------------------------------

Each plug-in binary is distributed as a Mac OS X package style directory
hierarchy. Note that there are two distinct meanings of 'bundle', one
referring to a binary file format, the other to a directory hierarchy
used to distribute software. We are distributing binaries in a bundle
package, and in the case of OSX, the binary is a binary bundle. All the
binaries must end with ``".ofx"``, regardless of the host operating
system.

The directory hierarchy is as follows.....

-  NAME.ofx.bundle

   -  Contents

      -  Info.plist
      -  Resources

         -  NAME.xml
         -  PLUGIN_A.png
         -  PLUGIN_A.svg
         -  PLUGIN_B.png
         -  PLUGIN_B.svg
         -  ...

      -  ARCHITECTURE_A

         -  NAME.ofx

      -  ARCHITECTURE_B

         -  NAME.ofx

      -  ...
      -  ARCHITECTURE_N

         -  NAME.ofx

Where...

-  Info.plist is relevant for OSX only and needs to be filled in
   appropriately,
-  NAME is the file name you want the installed plug-in to be identified
   by,
-  PLUGIN.png - is an optional portable graphics file image to use as an
   icon for the plug-in in the binary which has a matching
   pluginIdentifier
   field in the
   OfxPlugin
   struct,
-  PLUGIN.svg - is an optional scalable vector graphic file to use as an
   icon for the plug-in in the binary which has a matching
   pluginIdentifier
   field in the
   OfxPlugin
   struct,
-  ARCHITECTURE is the specific operating system architecture the
   plug-in was built for, these are currently...

   -  MacOS - for Apple Macintosh OS X 32 bit and/or universal binaries
   -  MacOS-x86-64 - for Apple Macintosh OS X, specifically on intel x86
      CPUs running AMD's 64 bit extensions. 64 bit host applications
      should check this first, and if it doesn't exist or is empty, fall
      back to "MacOS" looking for a universal binary.
   -  Win32 - for Microsoft Windows (compiled 32 bit)
   -  Win64 - for Microsoft Windows (compiled 64 bit)
   -  IRIX - for SGI IRIX plug-ins (compiled 32 bit)
   -  IRIX64 - for SGI IRIX plug-ins (compiled 64 bit)
   -  Linux-x86 - for Linux on x86 CPUs (compiled 32 bit)
   -  Linux-x86-64 - for Linux on x86 CPUs running AMD's 64 bit
      extensions

Note that not all the above architectures need be supported, at least
one.

This structure is necessary on OS X, but it also gives a nice skeleton
to hang all other operating systems from in a single install, as well as
a clean place to put resources.

The ``Info.plist`` is specific to Apple and you should consult the Apple
developer's website for more details. It should contain the following
keys...

-  CFBundleExecutable - the name of the binary bundle in the MacOS
   directory
-  CFBundlePackageType - to be 'BNDL'
-  CFBundleInfoDictionaryVersion
-  CFBundleVersion
-  CFBundleDevelopmentRegion

.. ArchitectureInstallingLocation:

Installation Location
---------------------

Plug ins are searched for in a variety of locations, both default and
user specified. All such directories are examined for plug-in bundles
and sub directories are also recursively examined.

A list of directories is supplied in the "OFX_PLUGIN_PATH" environment
variable, these are examined, first to last, for plug ins, then the
default location is examined.

On Microsoft Windows machines, the plug ins are searched for in,

1. the ';' separated directory list specified by the environment
   variable "OFX_PLUGIN_PATH"
2. the directory returned by
   getStdOFXPluginPath
   in the following code snippet...
   ::

       #include "shlobj.h"
       #include "tchar.h"
       const TCHAR *getStdOFXPluginPath(void)
       {
         static TCHAR buffer[MAX_PATH];
         static int gotIt = 0;
         if(!gotIt) {
           gotIt = 1;     
           SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES_COMMON, NULL, SHGFP_TYPE_CURRENT, buffer);
           _tcscat(buffer, __T("\\OFX\\Plugins"));
         }
         return buffer;       
       }
                 

3. the directory "C:\Program Files\Common Files\OFX\Plugins". This
   location is deprecated, and it is returned by the code snippet above
   on English language systems. However it should still be examined by
   hosts for backwards compatibility.

On Apple OSX machines, the plug ins are searched for in,

1. the ';' separated directory list specified by the environment
   variable "OFX_PLUGIN_PATH"
2. the directory "/Library/OFX/Plugins"

On UNIX, Linux and other UNIX like operating systems,

1. the ':' separated directory specified by the environment variable
   "OFX_PLUGIN_PATH"
2. the directory "/usr/OFX/Plugins"

Any bundle or sub-directory name starting with the character '@' is to
be ignored. Such directories or bundles must be skipped.

Plug-in Icons
-------------

Some hosts may wish to display an icon associated with a plug-in on
their interfaces. Any such icon must be in the Portable Network Graphics
format (see http://www.libpng.org/) and must contain 32 bits of colour,
including an alpha channel. Ideally it should be at least 128x128
pixels.

Host applications should dynamically resize the icon to fit their
preferred icon size. The icon should not have it's aspect changed,
rather the host should fill with some appropriate colour any blank areas
due to aspect mis matches.

Ideally plug-in developers should not render the plug-in's name into the
icon, as this may be changed by the resource file, especially for
internationalisation purposes. Hosts should thus present the plug-in's
name next to the icon in some way.

The icon file must be named as the corresponding pluginIdentifier field
from the OfxPlugin, post pended with '.png' and be placed in the
resources sub-directory.

Externally Specified Resources
------------------------------

Some plug-ins may supply an externally specified resource file.
Typically this is for tasks such as internationalising interfaces,
tweaking user interfaces for specific hosts and so on. These are XML
files and have DTD associated with the specific API, for example OFX
Image Effect DTD is found in ``ofx.dtd``.

The xml resource file is installed in the ``Resources`` sub directory of
the bundle hierarchy. It's name will be ``NAME.xml``, where name is the
base name of the bundle folder and the effect binaries.
