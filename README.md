# sdrplay-experiment
The purpose of this software is to collect simultaneous IQ data on one or more SDRPlay radios.

# requirements

- version 2.13 of the SDRPlay API for Linux.
- boost
- gnuplot-iostream (For quick data plotting) 
- Linux (Ubuntu recommended).
- An understanding of poorly done software and general computer savvy.

To build use a shell and enter the commands 

```
> git clone https://github.com/colbyAtCRI/sdrplay-experiment
> cd sdrplay-experiment
> cd build
> make
```

# utilities

- `sdr` runs N radios. Radios are configured using json files, see ./json for examples
- `phasediff` Plots the relative phase between two radio data sets versus time.
- `lsrsp` list the RSPs on the host machine.
- `fftcor` computes the correlation between two radios data sets.
- `fftplot` plots the power spectrum of a data set.
- `gfft` I forget. Plots something. Figure it out.
