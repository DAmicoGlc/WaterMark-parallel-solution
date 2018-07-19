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
#include "util.cpp"
#include "CImg.h"

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

// struct for information about chunk
typedef struct {

	unsigned char * ptrMarker;

	int imgNmbPx;
	int chunksRow;
	int markWidth;
	int offset;

} workerInfo, *workerInfoPtr;

// queue from scatter to worker for the information about chunks
queue<pair<unsigned char *,CImg<unsigned char> *> >  *workerQueue;

// queue from workers to gather
queue<pair<int, CImg<unsigned char> *> > gatherQueue;

/* SCATTER
INPUT:
	- number of workers
	- vector of pointer to the array of pixels of the images
	- vector of pointer to the images
Calculate the information about the partition of the image.
Only once push information to worker.
For each images:
	Push the pointer to the pixels array and the pointer to the image in each queues waiting 
		eventually a fixed time to emulate the inter-arrival time othe stream 


Push a null in each queues to propagate the EOF
*/
void scatter(int nw,vector<unsigned char *> dataImgVector,vector<CImg<unsigned char> *> imgVector) {

	// loop on the images vector
	for ( int j=0 ; j<imgVector.size() ; j++ ) {
		active_delay(interarrival_time);

		// loop on workers
		for ( int i=0 ; i<nw ; i++ ) 
			workerQueue[i].push( make_pair(dataImgVector[j],imgVector[j]) );

	}

	CImg<unsigned char> *eofImage = NULL;
	unsigned char *eofData = NULL;

	// push th end of stream
	for (int i = 0; i < nw; i++) {
		workerQueue[i].push( make_pair(eofData,eofImage) );
	}

	return;
  
}

/* 	WORKERS
INPUT:
	- index is needed to associate one worker to his own queue
	- infoChunk, information about its own chunk
Pop image from his own queue and process it
Push the number of the image processed and its pointer, the numbeer is needed to identify that image
*/
void worker(int index,workerInfoPtr infoChunk) {

	int imageCounter=0;

	pair<unsigned char *, CImg<unsigned char> *> ptrPair;

  	unsigned char *ptrDataImg;

	// thread have to work untill it find the end of stream (NULL)
	while(true) {

		ptrPair = workerQueue[index].pop();

		ptrDataImg = ptrPair.first;

		if ( ptrDataImg==NULL ) {

			// propagate EOS
			gatherQueue.push( make_pair(0,ptrPair.second) );

			// free memory
			delete infoChunk;
			return;
		}

	  	int greyValue, avgGreyValue, redOff, greenOff, blueOff, markOff;

	    // for loop on the number of pixel to process
	    for ( int y=0 ; y < infoChunk->chunksRow*infoChunk->markWidth ; y++) {

	    	// offset to identify the pixels
    		markOff = (infoChunk->offset+y);

    		// the marker could be not completely B&W, so use a threshold to identify black pixels
			if ( *(infoChunk->ptrMarker + markOff)  < 50 ) 
			{
				// RGB offset to know each value of the three colors
				redOff = markOff*sizeof(unsigned char);
				greenOff = (markOff + infoChunk->imgNmbPx ) * sizeof(unsigned char);
				blueOff = (markOff + 2*( infoChunk->imgNmbPx ) ) * sizeof(unsigned char);


				greyValue = ( *( ptrDataImg + redOff ) + *( ptrDataImg + greenOff ) + *( ptrDataImg + blueOff ) )/3;

				avgGreyValue = (greyValue+*(infoChunk->ptrMarker + markOff))/2;

				*(ptrDataImg + redOff ) = avgGreyValue;
				*(ptrDataImg + greenOff ) = avgGreyValue;
				*(ptrDataImg + blueOff ) = avgGreyValue;
			
			}
	    }

	    // propagate the result
	    gatherQueue.push( make_pair(imageCounter,ptrPair.second) );

	    imageCounter++;

	}

	return;
}

/* GATHER
INPUT:
	- number of worker
Create a map<int,int> (eg an hashtable)
Pop the result from the workers,
	if the pointer is NULL
		increase null counter
		if null counter is equal to number of worker
			return
	else
		if the number receive is already in the map, 
			if the value of that number is equal to nw-1
				i receive all parts of that image, save it and free memory
			else
				increase the value corrisponding to that image
		else
			insert the new key with value 1 in the map
This method guaratee to not lose parts of image different between what gather expect
(eg one worker is so fast to compute his part of second image before another worker finish his part of the first image)
*/
void gather(int nw) {

	map< int, int> pool;

	pair<int, CImg<unsigned char> *> workerResult;

	// initialize the output path
	string path="markedImageMap/img_";

	CImg<unsigned char> * ptrImg;

	int imageId;
	int nullNmb=0;

	while(true) {

		workerResult = gatherQueue.pop();

		ptrImg = workerResult.second;

		if (ptrImg==NULL) {
			// count number of null
			nullNmb++;
			if(nullNmb==nw)
				return;
		}
		else {
			imageId = workerResult.first;

			if ( pool.count(imageId) == 1 || nw==1 ) {
				
				if ( pool[imageId] == nw-1 ) {

					pool.erase(imageId);

					path +=to_string(imageId)+".jpg";

					//ptrImg->save(path.c_str());

					path="markedImageMap/img_";

					// free memory
					delete ptrImg;

				}
				else {
					pool[imageId]++;
				}
			}
			else {
				pool.insert(make_pair(imageId,1));
			}
		}
	}

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
Create the scatter thread, create the gather thread and "nw" worker threads and join them.

For test purposes:
	- in order to save memory in the example folder there is only one image which is loaded "imageNumber" times;
*/
int main(int argc, char const *argv[])
{

	if ( argc < 4 ) {
    	cout << "Usage is: " << argv[0] << " inputFolder waterMarkFileName nw " << std::endl;
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
	vector<unsigned char *> dataImgVector;
	vector<CImg<unsigned char> *> imgVector;

	if ((dir = opendir (inputFolder.c_str())) != NULL) {
	  /* print all the files and directories within directory */
	  while ((ent = readdir (dir)) != NULL) {

	  	if ( findJPG(ent->d_name) )
	  		imgStream.push_back(inputFolder+ent->d_name);
	    
	  }
	  closedir (dir);
	} else {
	  /* could not open directory */
	  perror ("");
	  return EXIT_FAILURE;
	}

	// load image form folder
	for ( int j=0 ; j<imageNumber ; j++ ) {

		CImg<unsigned char> *image = new CImg<unsigned char>();

		image->load(imgStream[0].c_str()); 						// change it with "j" to load different images

		imgVector.push_back(image);

		dataImgVector.push_back(image->data());

	}

	// load water mark
	CImg<unsigned char> *mark = new CImg<unsigned char>();
	mark->load(markName.c_str());

	auto start   = std::chrono::high_resolution_clock::now();

	vector<workerInfoPtr> infoVector;

	unsigned char *ptrMark = mark->data(); 
	int markHeight = mark->height();
	int markWidth = mark->width();
	int offset = 0;
	int imgNmbPx ,chunksRow ,lastRow;
	int rowAssigned = 0;

	imgNmbPx = markHeight*markWidth;

	chunksRow = (markHeight/nw)+1;
	lastRow = markHeight%nw;

	// loop on workers
	for ( int i=0 ; i<nw ; i++ ) {

		// last row say me how many row exceed from the split, when i assigned all of it i decrease the number of row to give to worker
		if ( lastRow == i )
			chunksRow=chunksRow-1;

		offset = rowAssigned*markWidth;

		workerInfoPtr workerParameter = new workerInfo{ ptrMark, imgNmbPx, chunksRow, markWidth, offset };

		rowAssigned +=chunksRow;

		infoVector.push_back(workerParameter);

	}

	// allocate memory for the worker queues
	workerQueue = new queue<pair<unsigned char *,CImg<unsigned char> *> >[nw];


	//Create the scatter and gather threads
	thread scatterThread(scatter,nw,dataImgVector,imgVector);
	thread gatherThread(gather, nw);

	vector<thread> workers;

	//Create worker threads
	for (int i = 0; i < nw; ++i) 
		workers.push_back(thread(worker, i, infoVector[i]));

	// Join threads togheter
	scatterThread.join();
	
	for (int i = 0; i < nw; ++i) 
		workers[i].join();

	gatherThread.join();

	delete mark;

	auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto msec    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

  	cout << "Parallel time: " << msec << "ms" << " for " << imageNumber << " images " << "using " << nw << " workers! (saving time is considered)" << endl;

	return 0;
}