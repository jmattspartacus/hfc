# HFC

HFC is a program used to time sort data from GRETINA experiments. It takes a Global.dat file with GRETINA Event Builder (GEB) data and uses the timestamps to sort it. 

I wanted to make this publicly available without searching through other repositories or having to move this code about on a flash drive, and to make sure a record of any changes is kept available for future users.

# Building

To build HFC, run `make`, aside from C++ development tools, development headers and shared libs for the [zlib](https://www.zlib.net/) and [bzip2](https://sourceware.org/bzip2/) compression libraries need to be available on the system path. 

This program currently will not compile on Windows, I would suggest using WSL if you cannot use a Unix/Linux based operating system. 

# Difference with historic HFC program

The version here uses a priority queue instead of a linked list. While it will use somewhat more memory, it runs about 3 times faster for large input files. 

For "very old" timestamps, these will now be located at the end of the output file. They are no longer discarded as they were with the old output files.

# Disclaimer

I make no claim to ownership of this code, but if something here is broken, I am willing to maintain it. 

From what I can tell, the original version comes from here https://github.com/GRETINA-LBNL/gretina-unpack 
