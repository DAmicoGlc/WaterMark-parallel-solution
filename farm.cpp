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
#define cimg_use_jpeg 1
#include "CImg.h"
#include "util.cpp"

using namespace std;
using namespace cimg_library;

// variable to set the interarrival time of the stream
int interarrival_time;
// variable to set the number of the image to process
int imageNumber;

// auxiliar function to print out result from threads
mutex mutexCOUT;
void printOut(string show) {
	unique_lock<mutex> lck(mutexCOUT);
	cout << show << endl;
}

// auxiliar function to find file with extention ".jpg"
int findJPG(string check) {
	string ext = ".jpg";
	if ( check.length() >= ext.length() ) 
		return ( 0 ==check.compare ( check.length()-ext.length() , ext.length() , ext ) );
	else
		return 0;
}

// struct of information pass between stages
typedef struct {
	unsigned char * image;
	int index;
} task;


// queue from emitter to workers
queue<task *>  workerQueue; 

/* EMITTER
INPUT:
	- number of workers
	- vector of image
Push the images in the queue wasting a fixes time to emulate the inter-arrival time of the stream.
Push a number of NULL equal to the number of workers to notify the enf of stream
*/
void emitter(int nw,vector<unsigned char *> imgVector) {

	for ( int j=0 ; j<imgVector.size() ; j++ ) {
		active_delay(interarrival_time);
		task *infoImage=new task{imgVector[j],j};
		workerQueue.push(infoImage);
	}

	for (int i = 0; i < nw; ++i)
		workerQueue.push(NULL);

	return;
}

/* WORKERS
INPUT:
	- pointer to marker image
Pop images from queue and process it.
Save the marked image in a fixed output folder.
Delete the pointer to image in order to free occupied memory
*/
void worker(CImg<unsigned char> * markImg) {

	int greyValue, avgGreyValue;

	int pixNmb=markImg->size()/3;

	task *imageInfo;

	unsigned char *mark=markImg->data();

	// initialize the output path
	string path="markedImageFarm/img_";

	// pop image from queue
	imageInfo = workerQueue.pop();

	// loop untill recive an EOF
	while(imageInfo!=NULL) {

		// loop over the pixels of the image which need to be marked
		for ( int x = 0 ; x < pixNmb ; x++ ) {

			// the marker could be not completely B&W, so use a threshold to identify black pixels
			if ( *(mark+ x ) < 50 ) 
			{
				greyValue = (*(imageInfo->image+ (x)* sizeof(unsigned char) ) + *(imageInfo->image+ (x + pixNmb)* sizeof(unsigned char) ) 
					+ *(imageInfo->image+ (x + pixNmb*2)* sizeof(unsigned char) ) )/3;

				avgGreyValue = (greyValue+*(mark+ (x)* sizeof(unsigned char)) )/2;

				*(imageInfo->image+ (x)* sizeof(unsigned char) ) = avgGreyValue;
				*(imageInfo->image+ (x + pixNmb)* sizeof(unsigned char) ) = avgGreyValue;
				*(imageInfo->image+ (x + pixNmb*2)* sizeof(unsigned char) ) = avgGreyValue;
			}
		}
		
		
		path +=to_string(imageInfo->index)+".jpg";

		(imageInfo->image)->save_jpeg(path.c_str());

		delete imageInfo;

		path="markedImageFarm/img_";

		// pop next one image
		imageInfo = workerQueue.pop();
	}

	// free memory
	delete markImg;
	return;

}


/* MAIN 
USAGE:
	Mandatory parameter:
		- input folder name plus "/"							->inputFolder
		- water mark name with extension						->markName
		- number of worker 										->nw
	Optional:
		- inter-arrival time of the stream 	(default:10us)		->interarrival_time
		- image number 						(default:100)		->imageNumber
List the existing file with ".jpg" extension in the input folder.
Load sequentially images from the list of file.
Load nw marker for the workers.
Create the emitter thread and "nw" worker threads and join them.

For test purposes:
	- in order to save memory in the example folder there is only one image which is loaded "imageNumber" times;
*/
int main(int argc, char const *argv[])
{

	if ( argc < 4 ) {
    	cout << "Usage is: " << argv[0] << " inputFolder waterMarkFileName nw interarrivalTime imageNumber (last 2 parameter are optional)" << std::endl;
    return(0);
  	}

	int nw = atoi(argv[3]);
	string inputFolder = argv[1];
	string markName = argv[2];

	if ( argc>=5 )
		interarrival_time = atoi(argv[4]);
	else
		interarrival_time = 10;

	if ( argc==6 )
		imageNumber = atoi(argv[5]);
	else
		imageNumber = 100;


	// list file in input folder
	DIR *dir;
	struct dirent *ent;

	vector<string> imgStream;
	vector<CImg<unsigned char> *> markerVec;
	vector<unsigned char *> imgDataVector;
	vector<CImg<unsigned char> *> imgVector;

	if ((dir = opendir (inputFolder.c_str())) != NULL) {
	  while ((ent = readdir (dir)) != NULL) {

	  	if ( findJPG(ent->d_name) )
	  		imgStream.push_back(inputFolder+ent->d_name);
	    
	  }
	  closedir (dir);
	} else {
	  perror ("");
	  return EXIT_FAILURE;
	}

	// load image form folder
	for ( int j=0 ; j<imageNumber ; j++ ) {

		CImg<unsigned char> *image = new CImg<unsigned char>();

		image->load(imgStream[0].c_str());						// change it with "j" to load different images

		imgDataVector.push_back(image->data());

		imgVector.push_back(image);
	}

	// load water mark
	for (int i = 0; i < nw; ++i)
	{
		CImg<unsigned char> *marker = new CImg<unsigned char>();
		marker->load(markName.c_str());
		markerVec.push_back(marker);
	}

	auto start = std::chrono::high_resolution_clock::now();

	//Create the emitter thread
	thread emitterThread(emitter,nw,imgDataVector);
	vector<thread> workers;

	//Create worker threads
	for (int i = 0; i < nw; ++i) {
		workers.push_back(thread(worker, markerVec[i]));
	}

	// Join threads togheter
	emitterThread.join();
	for (int i = 0; i < nw; ++i) {
		workers[i].join();
	}

	for (int i = 0; i < imageNumber; ++i) 
		delete imgVector[i];

	auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto msec    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

  	cout << "Parallel time: " << msec << "ms" << " for " << imageNumber << " images " << "using " << nw << " workers! (saving time is considered)" << endl;


	return 0;
}