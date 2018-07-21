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

	int imgNmbPx;
	int chunksRow;
	int markWidth;
	int offset;

} workerInfo, *workerInfoPtr;

// queue from scatter to worker for the information about chunks
queue<tuple<unsigned char *,workerInfoPtr,CImg<unsigned char> *,int> >  *workerQueue;

// queue from workers to gather
queue<pair<int, CImg<unsigned char> *> > *gatherQueue;

/* SCATTER
INPUT:
	- number of workers per map
	- number of map
	- vector of pointer to the array of pixels of the images
	- vector of pointer to the images
	- vector of information about the partition
Only once push information to worker.
For each images:
	Push the information about image and the partition in the queue of the map (round robin for each map) waiting 
		eventually a fixed time to emulate the inter-arrival time othe stream 

Push a one null for each worker in each queues to propagate the EOF
*/
void scatter(int nWorkerMap,int nmbMap,vector<unsigned char *> dataImgVector,vector<CImg<unsigned char> *> imgVector,vector<workerInfoPtr> infoChunk) {

	int mapIndex=0;

	// loop on the images vector
	for ( int j=0 ; j<imgVector.size() ; j++ ) {
		active_delay(interarrival_time);

		mapIndex=j%nmbMap;

		// loop on workers
		for ( int i=0 ; i<nWorkerMap ; i++ ) 
			workerQueue[mapIndex].push( make_tuple(dataImgVector[j],infoChunk[i],imgVector[j],j) );

	}

	CImg<unsigned char> *eofImage = NULL;
	unsigned char *eofData = NULL;
	workerInfoPtr eofChunk = NULL;

	// push th end of stream
	for (int i = 0; i<nmbMap ; i++ )
		for (int j = 0; j < nWorkerMap; ++j)
			workerQueue[i].push( make_tuple(eofData,eofChunk,eofImage,0) );
	

	return;
  
}

/* 	WORKERS
INPUT:
	- index is needed to associate one worker to his own queue
	- ptrMarker, pointer to the marker image
	- mapID, is neede to identify the map associated to the worker
Pop image from his own queue and process it
Push the number of the image processed and its pointer, the numbeer is needed to identify that image
*/
void worker(int index,int mapID,unsigned char *ptrMarker) {

	int imageCounter=0;

	workerInfoPtr infoChunk;

	tuple<unsigned char *,workerInfoPtr,CImg<unsigned char> *,int> ptrTuple;

  	unsigned char *ptrDataImg;

	// thread have to work untill it find the end of stream (NULL)
	while(true) {

		ptrTuple = workerQueue[mapID].pop();

		ptrDataImg = get<0>(ptrTuple);

		if ( ptrDataImg==NULL ) {

			// propagate EOS
			gatherQueue[mapID].push( make_pair(0,get<2>(ptrTuple)) );

			return;
		}

		infoChunk = get<1>(ptrTuple);

	  	int greyValue, avgGreyValue, redOff, greenOff, blueOff, markOff;

	    // for loop on the number of pixel to process
	    for ( int y=0 ; y < infoChunk->chunksRow*infoChunk->markWidth ; y++) {

	    	// offset to identify the pixels
    		markOff = (infoChunk->offset+y);

    		// the marker could be not completely B&W, so use a threshold to identify black pixels
			if ( *(ptrMarker + markOff)  < 50 ) 
			{
				// RGB offset to know each value of the three colors
				redOff = markOff*sizeof(unsigned char);
				greenOff = (markOff + infoChunk->imgNmbPx ) * sizeof(unsigned char);
				blueOff = (markOff + 2*( infoChunk->imgNmbPx ) ) * sizeof(unsigned char);


				greyValue = ( *( ptrDataImg + redOff ) + *( ptrDataImg + greenOff ) + *( ptrDataImg + blueOff ) )/3;

				avgGreyValue = (greyValue+*(ptrMarker + markOff))/2;

				*(ptrDataImg + redOff ) = avgGreyValue;
				*(ptrDataImg + greenOff ) = avgGreyValue;
				*(ptrDataImg + blueOff ) = avgGreyValue;
			
			}
	    }

	    // propagate the result
	    gatherQueue[mapID].push( make_pair(get<3>(ptrTuple),get<2>(ptrTuple)) );

	    imageCounter++;

	}

	return;
}

/* GATHER
INPUT:
	- number of worker
	- mapID, is needed to identify the map associated to the gather
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
void gather(int nw,int mapID) {

	map< int, int> pool;

	pair<int, CImg<unsigned char> *> workerResult;

	// initialize the output path
	string path="markedImageMap/img_";

	CImg<unsigned char> * ptrImg;

	int imageId;
	int nullNmb=0;
	int imgNumber=0;

	while(true) {

		workerResult = gatherQueue[mapID].pop();

		ptrImg = workerResult.second;

		if (ptrImg==NULL) {
			// count number of null
			nullNmb++;
			if(nullNmb==nw) {
				printOut("Map: "+to_string(mapID)+"- Process: "+to_string(imgNumber)+" images!");
				return;
			}
		}
		else {
			imageId = workerResult.first;

			if ( pool.count(imageId) == 1 || nw==1 ) {
				
				if ( pool[imageId] == nw-1 ) {

					pool.erase(imageId);

					path +=to_string(imageId)+".jpg";

					ptrImg->save(path.c_str());

					path="markedImageMap/img_";

					imgNumber++;

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
		- image number 						(default:100)		->imageNumber
		- inter-arrival time of the stream 	(default:10us)		->interarrival_time
List the existing file with ".jpg" extension in the input folder.
Load sequentially images from the list of file.
Load nw marker for the workers.
Create the scatter thread, create the gather thread and "nw" worker threads and join them.

For test purposes:
	- in order to save memory in the example folder there is only one image which is loaded only once
		and then it is copied imagenumber-1 times;
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
		imageNumber = atoi(argv[4]);
	else
		imageNumber = 100;

	if ( argc==6 )
		interarrival_time = atoi(argv[5]);
	else
		interarrival_time = 10;


	if (imageNumber==1) {
		cout << "Si prega di utilizzare più di una immagine!" << endl;
		return 0;
	}


	// calulate the number of map and the number of worker per map
	int nmbMap=1;
	int nmbProbMap;
	int nWorkerMap=nw;

	if (nWorkerMap>16) {
		nmbMap*=nWorkerMap/16;
		nWorkerMap = 16;
	}

	if ( nmbMap*2 > imageNumber ) {
		do {
			nWorkerMap*=2;
			nmbMap/=2;
		} while( nmbMap*2 > imageNumber );
	}	

	cout << "Numero map: " << nmbMap << " Numero worker per map: " << nWorkerMap << endl;

	// list file in input folder
	DIR *dir;
	struct dirent *ent;

	vector<string> imgStream;
	vector<unsigned char *> dataImgVector;
	vector<CImg<unsigned char> *> imgVector;
	vector<unsigned char *> dataMarkVector;
	vector<CImg<unsigned char> *> markVector;

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

	CImg<unsigned char> *image = new CImg<unsigned char>();     // modify to get all image in a folder
	image->load(imgStream[0].c_str());
	imgVector.push_back(image);
	dataImgVector.push_back(image->data()); 	

	// load image form folder
	for ( int j=1 ; j<imageNumber ; j++ ) {

		CImg<unsigned char> *image = new CImg<unsigned char>((*imgVector[0]));

		imgVector.push_back(image);

		dataImgVector.push_back(image->data());

	}

	CImg<unsigned char> *mark = new CImg<unsigned char>();
	mark->load(markName.c_str());
	markVector.push_back(mark);
	dataMarkVector.push_back(mark->data()); 	

	// load image form folder
	for ( int j=1 ; j<nmbMap ; j++ ) {

		CImg<unsigned char> *mark = new CImg<unsigned char>((*markVector[0]));

		markVector.push_back(mark);

		dataMarkVector.push_back(mark->data());

	}

	// calculate the info chunk for each worker of a map
	vector<workerInfoPtr> infoVector;	

	int markHeight = markVector[0]->height();
	int markWidth = markVector[0]->width();
	int offset = 0;
	int imgNmbPx ,chunksRow ,lastRow;
	int rowAssigned = 0;

	imgNmbPx = markHeight*markWidth;

	chunksRow = (markHeight/nWorkerMap)+1;
	lastRow = markHeight%nWorkerMap;
	rowAssigned=0;
	offset = 0;

	// loop on workers
	for ( int i=0 ; i<nWorkerMap ; i++ ) {

		// last row say me how many row exceed from the split, when i assigned all of it i decrease the number of row to give to worker
		if ( lastRow == i )
			chunksRow=chunksRow-1;

		offset = rowAssigned*markWidth;

		workerInfoPtr workerParameter = new workerInfo{ imgNmbPx, chunksRow, markWidth, offset };

		rowAssigned +=chunksRow;

		infoVector.push_back(workerParameter);

	}

	auto start   = std::chrono::high_resolution_clock::now();

	// allocate memory for the worker queues
	workerQueue = new queue<tuple<unsigned char *,workerInfoPtr,CImg<unsigned char> *,int> >[nmbMap];

	gatherQueue = new queue<pair<int, CImg<unsigned char> *> >[nmbMap];


	//Create the scatter and gather threads
	thread scatterThread(scatter,nWorkerMap,nmbMap,dataImgVector,imgVector,infoVector);

	vector<thread> workers;
	vector<thread> gathers;

	for (int i = 0; i < nmbMap; ++i) {
		gathers.push_back(thread(gather,nWorkerMap,i));
		for (int j = 0; j < nWorkerMap; ++j) 
			workers.push_back(thread(worker, (j+nWorkerMap*i),i,dataMarkVector[i]));
	}

	// Join threads togheter
	scatterThread.join();
	
	for (int i = 0; i < nw; ++i) 
		workers[i].join();

	for (int i = 0; i < nmbMap; ++i)
		gathers[i].join();

	auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto usec    = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();

    for (int i = 0; i < nmbMap; ++i)
		delete markVector[i];

  	cout << "Parallel time: " << usec << "us, for " << imageNumber << " images, using " << nw << " workers!" << endl;

	return 0;
}