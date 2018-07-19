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
	vector<unsigned char *> imageDataToMark;

	for (int i = 0; i < imageNumber ; ++i) {
		CImg<unsigned char> *image = new CImg<unsigned char>();
		image->load( imgStream[0].c_str() );						// change it with "j" to load different images
		imageToMark.push_back(image);
		imageDataToMark.push_back(image->data());
	}

	// SEQUENTIAL CODE

	CImg<unsigned char> markSeq(mark.c_str());
	unsigned char *ptrMark=markSeq.data();

	int heightOffset, widthOffset;
	int heightImage, widthImage, heightMark, widthMark;
	int greyValue, avgGreyValue;
	int pixNmb;

	// extract information about images
	heightMark = markSeq.height();
	widthMark = markSeq.width();

	pixNmb=heightMark*widthMark;

	// initialize the output path
	string path="markedImage/img_";

	auto start   = std::chrono::high_resolution_clock::now();

	for (int i = 0; i < imageToMark.size(); ++i)
	{
		// loop over the pixels of the image which need to be marked
		for ( int x = 0 ; x < pixNmb ; x++ ) {
			// the marker could be not completely B&W, so use a threshold to identify black pixels
			if ( *(ptrMark+x* sizeof(unsigned char)) < 50 ) 
			{
				greyValue = (*(imageDataToMark[i]+ (x)* sizeof(unsigned char) ) + *(imageDataToMark[i]+ (x + pixNmb)* sizeof(unsigned char) ) 
					+ *(imageDataToMark[i]+ (x + pixNmb*2)* sizeof(unsigned char) ) )/3;

				avgGreyValue = (greyValue+*(ptrMark+x* sizeof(unsigned char)) )/2;

				*(imageDataToMark[i]+ (x)* sizeof(unsigned char) ) = avgGreyValue;
				*(imageDataToMark[i]+ (x + pixNmb)* sizeof(unsigned char) ) = avgGreyValue;
				*(imageDataToMark[i]+ (x + pixNmb*2)* sizeof(unsigned char) ) = avgGreyValue;
			
			}
		}
		

		
		path +=to_string(i)+"_Seq.jpg";

		//imageToMark[i]->save(path.c_str());

		path="";
	}

	auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto usec    = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();

    for (int i = 0; i < imageNumber; ++i)
    	delete imageToMark[i];

  	cout << "Sequential time: " << usec << "us" << " for " << imageNumber << " images!" << endl;

  	return 0;

}