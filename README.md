# WaterMark-parallel-solution
WATER MARK PARALLEL SOLUTION

Given an image stream of fixed size and a water mark, each version of the program stamp the mark on the images.
There are 7 different version:
- Sequential;
- Farm;
- Map;
- Farm of Map;
- FF_Farm;
- FF_Map;
- FF_Farm_Map;

To compile:
	make versionName (eg farm...ff_farm..etc)
To run:
	./versionName inputFolder marker.jpg parallelismDegree ImageNumber

In the folder test/ the are the same version with some code to extract information about execution time.

In the folder of the image there is only one image which is loaded 1 time and copied "ImageNumber" time,
in order to don't waste time for the reading on disk. It is easy to change the code in order to load each
image in the input folder.

The CImg library is utilized in order to manage the images.
