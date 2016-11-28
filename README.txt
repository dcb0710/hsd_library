*** Introduction ***

This library is a set of external objects for Pure Data and was created during my Bachelor Thesis at the University of Applied Sciences Duesseldorf („Hochschule Duesseldorf“ = HSD).
It is a collection of basic DSP-Algotithms and has two main purposes:
- Showing the basics of creating Pd-Externals
- Demonstrating the implementation of common DSP-Algorithms in C

Therefore, this library can and should be used as a starting point for the development of new externals.
The externals were originally designed to run on a Raspberry Pi, but most of those externals are common DSP algorithms and work platform independent, so I decided to upload them as a small library. Maybe someone can benefit from it ;)

The library is based on the library template:
https://puredata.info/docs/developer/LibraryTemplate



*** License ***

 This library is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 There is a copy of the GNU General Public License attached to this repository.



*** Contents ***

*PD Externals*

NOTE: 
For further information about the usage of an particular external, see the attached help-patches („-help.pd“). 
For further information about the DSP-algorithms and code stuff, there should be a small description inside each C-File. 
For further information about C-Externals in general, see the code demos in the code-examples-Folder.

Filters:

hsd_biquad~
A simple Biquad filter. It can be specified as Lowpass, Highpass, Bandpass, Bandreject(Notch) and Allpass. Where both frequency and Q/resonance can be specified. It uses the calculation of coefficients from the „DAFX“ Book by Udo Zölzer. 

hsd_biquad_engine~
Based on the hsd_biquad~, but the complete calculation of coefficients is omitted. Instead, the coefficients can be set via creation arguments or seperate inlets. NOTE: If you specify the coefficients (which are quite long numbers) via creation arguments, be aware that pure data truncates float numbers when re-opening a patch!

hsd_biquad_coefficents
This is a non-signal processing external. It takes a filter-type, frequency and Q/resonance as arguments and gives out the five relevant coeffcients via seperate outputs. It can be understood as the counterpart of the hsd_biquad_engine~-object. Together they have the same capability as the hsd_biquad~.

hsd_svf~
State Variable Filter. A standard filter object for (simultaneous) lowpass, highpass and bandpass output. Unlike the biquad filter, the cutoff frequency and resonance are directly related to the filter parameters, so theres no set of filter coefficients that has to be recalculated whenever a parameter changes. This filter is better suited or time varying applications, like in synthesizers.

Delay-based effects:

hsd_delay~
A simple delay line. The delay time can be specified in ms. This delay line is the basis for all delay-based effects.

hsd_vibrato~
Not very different to hsd_delay~, but the delay time is constantly modulated by a sinusoidal LFO. This means that the ring buffer is played back with a variable speed. Like with a tape machine, this results in a pitch modulation. In combination with the dry signal, it can produce flanging effects.

hsd_chorus~
This is an implementation of the „Stereo Quadrature Chorus“ from the Book „Designing Audio Effect Plug-Ins in C++“ by Will Pirkle. It is a stereo effect, consisting of two modulated delay lines (like hsd_vibrato~) with different delay times. The LFO frequencies are the same but the one channel has a 90° phase shift.

hsd_comb~
Comb filter object (a delay line with feedback path), resulting in an echo-type audio effect. Derived from the Book „Designing Audio Effect Plug-Ins in C++“ by Will Pirkle. 

hsd_comblp~
Similar to the hsd_comb~ object, but there is a one pole lowpass filter in the feedback path. The result is that high frequencies are losing energy faster than the whole signal with every iteration.

hsd_allpass~
A simple allpass filter (acutally a comb filter with an additional feed forward path), derived from the Book „Designing Audio Effect Plug-Ins in C++“ by Will Pirkle.


Dynamics:

hsd_peakf~ & hsd_rmsf~
Two envelope follower objects. They generate an amplitude envelope of a given input signal, based on either a RMS- or Peak-approach. Both are an implementation of the envelope follower algorithms from the DAFX book by Udo Zoelzer.   


Helpers:

hsd_impulse~
Very simple impulse generator. When receiving a bang-message, it will output a impulse with a length of N samples. N can be defined by the user. 



*** Building / Usage of the library template ***


The basis for this library was the template from the Pd Community (https://puredata.info/docs/developer/LibraryTemplate). I omitted a lot of this template (like shared code files and libdir install option) to make the hsd-library as simple as possible. Nevertheless, I recommend to have a look at the link above to get some Informations about the template.

Contents of the Library:
- externals(folder): Inside this folder are all the single externals of the library as C-files with their corresponding help-files
- code-examples(folder): Not directly related to the hsd_library, these files should give a little help about the structure of an external. „helloworld.c“ is the ultimate minimalistic external, while „signaltemplate~.c“ gives some deeper information about (signal processing) externals. 
- manual(folder): contains a text file that should contain an instructional manual for the library. This is already done within this readme, so the manual is actually redundant. But somehow it is needed by the makefile for the automatic installation. 
- hsd_library-meta.pd: A Pd patch that contains some information about the library. It should be installed with the library and is embedded in every help-patch.
- LICENSE.txt: The Gnu Public License, under which the template is released and so are the externals of the hsd-library!
- README.md: The file you are currently reading
- Makefile: The guts of the library building process. Inside are all compiler instructions needed to build the C-files into loadable externals. It has several options, some are just platform dependent, some can be specified by the user. In the upper section of the makefile, the author of the library can (and must) specify some points, for example the name of the library, the associated externals, additional files or external libraries to link against. 


(Note: I made a small change in the makefile. I completely disabled the „< OPT_CFLAGS = -ftree-vectorize -ftree-vectorizer-verbose=2 -fast >
 - option“ of the mac_os target, because it caused errors while compiling under MacOSX Yosemite & El Capitan)




Building the externals:
As mentioned above, I omitted some files & parts of the original LibraryTemplate. This also reduces the capabilities of building, packaging & installing. While the original template is capable of some very cool things, like building into a single lib or using shared code, the only thing that works with this library is the simple building and automated copying of the single externals. 
- Open the terminal
- Navigate to the library folder (via „cd“)
- Type „make“ and hit enter to start the compiling process
- Inside the externals-folder, the single externals should pop up (also the corresponind .o-files, but these are only intermediate products of the build processed)
- Now the externals and their help-patches can be copied to a folder Pd knows (You can specify the search paths for pure data in its preferences). A common folder might be ~/Library/Pd. When you start Pd now, it should be capable of loading the external.
- Instead of a single „make“ there is a „make install“ command, which compiles the externals, and copies them with their help-patches, the manual folder and the meta file to the ~/Library/Pd directory.  
- To clean up the directory, type „make distclean“. All the files that were produced by the makefile are now deleted again





*** More Information ***

Most DSP Algorithms are taken from those three books:

- „DAFX“ by Udo Zoelzer (Editor), 2011, ISBN-13 978-0470979679
A large compendium for audio effects, where many Authors of the DAFX-Group present a introduction to their area of expertise. 
- „Digitale Audiosignalverarbeitung“ by Udo Zoelzer, 1995, ISBN-13: 978-3519261803
A standart book for DSP at my Institute. Some of the algorithms presentet in the DAFX book are based on this book. Unfortunately, only available in German language.
- „Designing Audio Effect Plug-Ins in C++“ by Will Pirkle, 2012, ISBN-13: 978-0240825151

For further information about writing externals, I can recommend this book:

- „Designing Audio Objects for Max/MSP and Pd“ by Eric Lyon, 2012, ISBN-13: 978-0895797155
A very interesting and good tutorial for writing externals. Mainly focused on Max/MSP Objects, but the differences to Pure Data Externals (they actually don´t differ that much!) are always pointed out.

If you don´t have the time or money to get a whole book, there is a very good tutorial by „IOhannes m Zmölnig“ (I think thats his artist pseudonym):
- „HOWTO write an External fir PureData“: http://pdstatic.iem.at/externals-HOWTO/, 2014, also downloadable as PDF

Unfortunately, there are not any more (usable) resources for writing Externals, though it is such a powerful way to extend Pd to anything.