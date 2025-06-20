# HFC

HFC is a program used to time sort data from GRETINA experiments. It takes a Global.dat file with GRETINA Event Builder (GEB) data and uses the timestamps to sort it. 

I wanted to make this publicly available without searching through other repositories or having to move this code about on a flash drive, and to make sure a record of any changes is kept available for future users.

# Building

To build HFC, run `make`, aside from C++ development tools, development headers and shared libs for the [zlib](https://www.zlib.net/) and [bzip2](https://sourceware.org/bzip2/) compression libraries need to be available on the system path. 

This program currently will not compile on Windows, I would suggest using WSL if you cannot use a Unix/Linux based operating system. 

# Differences with historic HFC program

Filetypes for input and output are now deduced for input and output (see [Flags Added](#Flags-added)) and bad filetypes will cause early exit.

The version here uses a priority queue instead of a linked list. While it will use somewhat more memory, it runs about 3 times faster for large input files. The increase in performance can become more substantial for very pathological files.

For "very old" timestamps, these will now be located at the end of the output file. They are no longer discarded as they were with the old output files.

## Flags added

* Event cropping: the `-c` flag enables event cropping, which removes zero padding at the end of mode 1 and mode 2 (types 1 and 3 respectively) decomposed data due to empty interaction points.
* Time windowing: the `-t` flag allows to configure the minimum window length in seconds (defaulted to 3 seconds) between the timestamp of the most next event write and the most recently read event. (Ex `-t 10.0`)
* Output Path: the `-o` flag allows specifying the name and path of the output file. (Ex `-o path/to/myfiles.dat.gz`)

## Flags Deprecated

* Compression flags: `-bz` and `-z` are deprecated, as the filetypes are now deduced from the file extensions of the input and output files.

# Disclaimer

I make no claim to ownership of this code, but if something here is broken, I am willing to maintain it. Updates and feature additions may be made from time to time as needed by myself or collaborators.

From what I can tell, the original version comes from here https://github.com/GRETINA-LBNL/gretina-unpack 

# Funding Acknowledgement

This work is supported in part by the U.S. Department of Energy, Office of Science, Office of Workforce Development for Teachers and Scientists, Office of Science Graduate Student Research (SCGSR) program. The SCGSR program is administered by the Oak Ridge Institute for Science and Education (ORISE) for the DOE. ORISE is managed by ORAU under contract number DE- SC0014664. All opinions expressed in herein are the author’s and do not necessarily reflect the policies and views of DOE, ORAU, or ORISE.
