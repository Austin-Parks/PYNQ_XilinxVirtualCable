# License
"xvcServer.c", is a derivative of "xvcd.c" (https://github.com/tmbinc/xvcd) 
by tmbinc, used under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/). 
"xvcServer.c" is licensed under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/) 
by Avnet and is used by Xilinx for XAPP1251.

# Overview
# Modified by Austin Parks
# Configured to wor with PYNQ Oen source Platform (Xilinx/AMD Embedded SOC Devices)

This forkk uses a modifed version of the mmap mode from XAP1251. I changed the IO device file target to /dev/mem (with an absolute offset) instead of using the generic_uio drivers setup in the examples. Yes I know this is dangerous, but here me out... This allows you to bypass the need for instanteating the generic_uio device drivers via device tree instanteations. Just plop a Debug bridge in AXI-to-BSCAN mode into your bitstream design and the pynq_xvc_utils.py module should be able to discover the debug bridge IP in your Overlay and automatically launch the xvcServer program!

# Smartlinks for everyone ! No more sharing ! Your Welcome [^_^]_/

Like the original XAP1251, it uses the mmap() system call to map IP memory into Linux user space, we're just using /dev/mem with an offset instead...

Other than this difference in IP memory access, the two modes share a common XVC server implementation.

Note: I removed all of the existing precompiler #if #else statements as they are just in the way at this point... 

## How to Cross-Compile
**The Makefile has a default cross-compiler of aarch64-linux-gnu-gcc and assumes this cross-compiler is available in the PATH when running make.**
Running the default
`make`
or
`make mmap`
will cross-compile xvcServer.c in mmap mode.  The program will be named ./xvcServer.

Running
`make clean`
will remove any of these programs cross-compiled by the Makefile.