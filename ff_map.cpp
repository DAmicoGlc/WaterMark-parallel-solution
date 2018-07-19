#include <iostream>
#include <string>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <vector>
#include <thread>
#include <test/ff/farm.hpp>
#define cimg_use_jpeg 1
#include "CImg.h"
#include "util.cpp"

using namespace std;
using namespace cimg_library;
using namespace ff;

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

// define task type
typedef struct {
	unsigned char *ptrDataImage;
	CImg<unsigned char> *ptrImage;
} taskWorker;

// define task type
typedef struct {
	int imageId;
	CImg<unsigned char> *ptrImage;
} taskGather;


/* SCATTER
INPUT:
	- number of workers
	- vector of pointer to the array of pixels of the images
	- vector of pointer to the images
Calculate the information about the partition of the image.
Only once push information to worker.

For each images:
	wait eventually a fixed time to emulate the inter-arrival time of the stream 
	Push the pointer to the pixels array and the pointer to the image to each worker


Push the EOF
*/
struct Scatter_Map: ff_node_t<char,taskWorker> {

	Scatter_Map(vector<CImg<unsigned char> *> imgStream, vector<unsigned char *> imgDataStream, int nw):imgStream(imgStream),imgDataStream(imgDataStream),nw(nw) { }

	taskWorker * svc(char *){

		for (int i = 0; i < imgStream.size(); ++i) {
			active_delay(interarrival_time);
			taskWorker *imagePtr= new taskWorker {imgDataStream[i],imgStream[i]};
			for (int i = 0; i < nw; ++i)
				ff_send_out(imagePtr);
		}

		// propagate EOS
		return EOS;
	}

	int nw;
	vector<CImg<unsigned char> *> imgStream;
	vector<unsigned char *> imgDataStream;
};

/* 	WORKERS
INPUT:
	- infoChunk, information about its own chunk
Pop image from his own queue and process it
Push the number of the image processed and its pointer, the numbeer is needed to identify that image
*/
struct Worker_map: ff_node_t<taskWorker,taskGather> {

	Worker_map(workerInfoPtr infoChunk): infoChunk(infoChunk) { imageCounter=0; }

	taskGather * svc(taskWorker *imageInfo) {

		if (imageInfo) {

			ptrDataImg = imageInfo->ptrDataImage;

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

		    imageCounter++;

		    taskGather *infoGather = new taskGather{imageCounter,imageInfo->ptrImage};

		    delete imageInfo;

		    // propagate the result
			return infoGather;
		}
		else
			return EOS;
	}

	workerInfoPtr infoChunk;
	unsigned char *ptrDataImg;
	int greyValue, avgGreyValue, markOff, redOff, greenOff, blueOff;
	int width, height;
	int imageCounter;
	int index;
	string path;
};

/* GATHER
INPUT:
	- number of worker
In the main was created an array to emulate a map (ff have some conflict with the library map) (eg an hashtable)
Pop the result from the workers,
	if the task is not NULL
		Increase the value of the array with address (imageId)
		if the value of the array with address (imageId) is equal to nw
			i receive all parts of that image, save it and free memory
	else
		increase null counter
		if null counter is equal to number of worker
			return
This method guaratee to not lose parts of image different between what gather expect
(eg one worker is so fast to compute his part of second image before another worker finish his part of the first image)
*/
struct Gather_Map: ff_node_t<taskGather,char> {

	Gather_Map(int nw,vector<int> pool):nw(nw),pool(pool) { nullNmb=0; path="markedImageFFmap/img_"; }

	char *svc(taskGather *workerResult){

		if(workerResult) {
			ptrImg = workerResult->ptrImage;
			imageId = workerResult->imageId;

			pool[imageId]++;
				
			if ( pool[imageId] == nw ) {

				path +=to_string(imageId)+".jpg";

				ptrImg->save(path.c_str());

				path="markedImageFFmap/img_";

				// free memory
				delete workerResult;
			}
		}
		else {
			// count number of null
			nullNmb++;
			if(nullNmb==nw)
				return EOS;
		}
		return GO_ON;
	}

	int nw;
	vector<int> pool;
	int nullNmb;
	int imageId;
	string path;
	CImg<unsigned char> *ptrImg;
};

/* MAIN 
USAGE:
	Mandatory parameter:
		- input folder name plus "/"							->inputFolder
		- water mark name with extension						->markName
		- number of worker 										->nw
	Optional:
		- image number 						(default:100)		->imageNumber
		- inter-arrival time of the stream 	(default:10us)		->interarrival_time
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
    	cout << "Usage is: " << argv[0] << " inputFolder waterMarkFileName nw interarrivalTime imageNumber (last 2 parameter are optional)" << std::endl;
    return(0);
  	}

	int nw = atoi(argv[3]);
	string inputFolder = argv[1];
	string markName = argv[2];

	if ( argc>=5 )
		imageNumber = atoi(argv[4]);
	else
		imageNumber = 100;

	if ( argc==6 )
		interarrival_time = atoi(argv[5]);
	else
		interarrival_time = 10;


	// list file in input folder
	DIR *dir;
	struct dirent *ent;

	vector<string> imgStream;
	vector<unsigned char *> dataImgVector;
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

		image->load(imgStream[0].c_str()); 						// change it with "j" to load different images

		imgVector.push_back(image);

		dataImgVector.push_back(image->data());

	}

	// load water mark
	CImg<unsigned char> *mark = new CImg<unsigned char>();
	mark->load(markName.c_str());

	auto start   = std::chrono::high_resolution_clock::now();

	// prepare information of partition, overhead of the map
	vector<workerInfoPtr> infoChunk;

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

		infoChunk.push_back(workerParameter);

	}

	vector<int> pool(imageNumber,0);

	// create the scatter node and the gather node
	Scatter_Map emitter(imgVector,dataImgVector,nw);
	Gather_Map gather(nw,pool);

	// create workers node
	vector<ff_node *> W;

	for (int i = 0; i < nw; ++i) {
		W.push_back(new Worker_map(infoChunk[i]));
	}

	// crate the farm
	ff_farm<> farm;

	// add nodes to the farm
	farm.add_emitter(&emitter);
	farm.add_collector(&gather);
	farm.add_workers(W);

	// start farm
	if(farm.run_and_wait_end() < 0 ) error ("running Farm\n");

	delete mark;

	for (int i = 0; i < imageNumber; ++i)
		delete imgVector[i];

	auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto msec    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

  	cout << "Parallel time: " << msec << "ms" << " for " << imageNumber << " images " << "using " << nw << " workers! (saving time is considered)" << endl;

	return 0;
}