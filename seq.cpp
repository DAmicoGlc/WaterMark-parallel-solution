#include <iostream>
#include <string>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <vector>
#include <thread>
#include <map>
#include <dirent.h>
#include "CImg.h"

using namespace std;
using namespace cimg_library;

// variable to set the number of the image to process
int imageNumber;

// auxiliar function to find file with extention ".jpg"
int findJPG(string check) {
	string ext = ".jpg";
	if ( check.length() >= ext.length() ) 
		return ( 0 ==check.compare ( check.length()-ext.length() , ext.length() , ext ) );
	else
		return 0;
}

/* MAIN 
USAGE:
	Mandatory parameter:
		- input folder name plus "/"							->inputFolder
		- water mark name with extension						->markName
	Optional:
		- image number 						(default:100)		->imageNumber
List the existing file with ".jpg" extension in the input folder.
Load sequentially images from the list of file.
Process and save sequeantially all the image in the vector

For test purposes:
	- in order to save memory in the folder there is only one image which is loaded "imageNumber" times;
*/

int main(int argc, char const *argv[])
{

	if(argc<3) {
		cout << " Usage inputFoleder/ markFileName" << endl; 
		return 0;
	}

	string inputFolder = argv[1];
	string mark = argv[2];

	if ( argc==4 )
        imageNumber = atoi(argv[3]);
    else
        imageNumber = 100;

    // list file in input folder
	DIR *dir;
	struct dirent *ent;

	vector<string> imgStream;

	if ((dir = opendir (inputFolder.c_str())) != NULL) {
		// store all the files names within directory 
		while ((ent = readdir (dir)) != NULL) {
			if ( findJPG(ent->d_name) )
				imgStream.push_back(inputFolder+ent->d_name);
		}
		closedir (dir);
	} 
	else {
		// could not open directory
		perror ("");
		return EXIT_FAILURE;
	}

	// get the image from the folder
	vector<CImg<unsigned char> *> imageToMark;

	for (int i = 0; i < imageNumber ; ++i) {
		CImg<unsigned char> *image = new CImg<unsigned char>();
		image->load( imgStream[0].c_str() );						// change it with "j" to load different images
		imageToMark.push_back(image);
	}

	// SEQUENTIAL CODE

	CImg<unsigned char> markSeq(mark.c_str());

	auto start   = std::chrono::high_resolution_clock::now();

	int heightOffset, widthOffset;
	int heightImage, widthImage, heightMark, widthMark;
	int greyValue, avgGreyValue;

	// extract information about images
	heightMark = markSeq.height();
	widthMark = markSeq.width();

	// initialize the output path
	string path="markedImage/img_";

	for (int i = 0; i < imageToMark.size(); ++i)
	{
		// loop over the pixels of the image which need to be marked
		for ( int y = 0 ; y < heightMark ; y++ ) {
			for ( int x = 0 ; x < widthMark ; x++ ) {
				// the marker could be not completely B&W, so use a threshold to identify black pixels
				if ( markSeq(x,y,0) < 50 ) 
				{
					greyValue = ((*imageToMark[i])(x,y,0,0) + (*imageToMark[i])(x,y,0,1) + (*imageToMark[i])(x,y,0,2) )/3;

					avgGreyValue = (greyValue+markSeq(x,y,0))/2;

					(*imageToMark[i])(x,y,0,0) = avgGreyValue;
					(*imageToMark[i])(x,y,0,1) = avgGreyValue;
					(*imageToMark[i])(x,y,0,2) = avgGreyValue;
				
				}
			}
		}

		
		path +=to_string(i)+"_Seq.jpg";

		//imageToMark[i]->save(path.c_str());

		// free memory
		delete imageToMark[i];

		path="";
	}

	auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto msec    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

  	cout << "Sequential time: " << msec << "ms" << " for " << imageNumber << " images! (saving time is considered)" << endl;

  	return 0;

}