# HFC

HFC is a program used to time sort data from GRETINA experiments. It takes a Global.dat file with GRETINA Event Builder (GEB) data and uses the timestamps to sort it. 

I wanted to make this publicly available without searching through other repositories or having to move this code about on a flash drive, and to make sure a record of any changes is kept available for future users.

# Building

To build HFC, run `make`, no external dependencies are needed, aside from C++ development tools. 

This program currently will not compile on Windows, I would suggest using WSL if you cannot use a Unix/Linux based operating system. 

# Disclaimer

I make no claim to ownership of this code, but if something here is broken, I am willing to maintain it. 
