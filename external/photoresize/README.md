# PhotoResize
A single-purpose extension for Tcl to resize/resample photo images

Use the standard `./configure; make; make install` incantation to build this package. 

PhotoResize uses a high-quality (antialiased) photo image rescaling algorithm derived from the netpbm packge. It only exports a single command, `resizephoto` which resamples an existing photo image and copies the resampled data to a target image. Sample code is like this:

	package require Img
	package require photoresize
	toplevel .d
	image create photo orig -file any_jpg.jpg
	puts "Loaded JPEG"
	image create photo disp

	pack [label .d.l -image disp]

	set time [time {resizephoto orig disp 600 400}]
	puts "Time for resampling: $time"

In-place resampling is allowed, i.e. source and target can refer to the same image.

In version 0.2 an enhanced command was added to resample a subsection of the source image. See the image viewer demonstration in the demo folder. 
