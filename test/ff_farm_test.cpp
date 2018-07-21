#include <iostream>
#include <string>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <vector>
#include <thread>
#include <ff/farm.hpp>
#define cimg_use_jpeg 1
#include "CImg.h"
#include "util.cpp"

using namespace std;
using namespace cimg_library;
using namespace ff;

vector<long> workerTime;

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

typedef struct {
	CImg<unsigned char> *ptrImage;
	unsigned char * image;
	int index;
} task;


/* EMITTER
INPUT:
	- number of workers
	- vector of image
	- vector of array of pixel
Push the images in the queue wasting a fixes time to emulate the inter-arrival time of the stream.
Push a number of NULL equal to the number of workers to notify the enf of stream
*/
struct Emitter_farm: ff_node_t<char,task> {

	Emitter_farm(vector<unsigned char *> imgDataStream,vector<CImg<unsigned char> *> imgStream, int nw):imgDataStream(imgDataStream),imgStream(imgStream),nw(nw) { }

	task *svc(char *){
		for (int i = 0; i < imgStream.size(); ++i) {
			active_delay(interarrival_time);
			task *imageInfo=new task{imgStream[i],imgDataStream[i],i};
			ff_send_out(imageInfo);
		}
		return EOS;
	}

	int nw;
	vector<unsigned char *> imgDataStream;
	vector<CImg<unsigned char>*> imgStream;
};

/* WORKERS
INPUT:
	- pointer to marker image
Pop images from queue and process it.
Save the marked image in a fixed output folder.
Delete the pointer to image in order to free occupied memory
*/
struct Worker_farm: ff_node_t<task,char> {

	Worker_farm(CImg<unsigned char> *markImg,int index): markImg(markImg),index(index) {
		// extract information about images
		pixNmb=markImg->size()/3;
		mark = markImg->data();
		nmbImg=0;
	}

	char *svc(task *imageInfo) {

		auto startWorker = std::chrono::high_resolution_clock::now();

		if(imageInfo) {

			nmbImg++;

			// initialize the output path
			path="markedImageFFfarm/img_";

			// loop over the pixels of the image which need to be marked
			// loop over the pixels of the image which need to be marked
			for ( int x = 0 ; x < pixNmb ; x++ ) {

				// the marker could be not completely B&W, so use a threshold to identify black pixels
				if ( *(mark+x) < 50 ) 
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

			//(imageInfo->ptrImage)->save_jpeg(path.c_str());

			auto elapsedWorker = std::chrono::high_resolution_clock::now() - startWorker;
			auto usecWorker    = std::chrono::duration_cast<std::chrono::microseconds>(elapsedWorker).count();

			workerTime[index]+=usecWorker;

			return GO_ON;
		}
		else {

			auto elapsedWorker = std::chrono::high_resolution_clock::now() - startWorker;
		    auto usecWorker    = std::chrono::duration_cast<std::chrono::microseconds>(elapsedWorker).count();

		    workerTime[index]+=usecWorker;

			return EOS;
		}
	}


	CImg<unsigned char> * markImg;
	unsigned char *mark;
	int greyValue, avgGreyValue;
	int pixNmb;
	int index;
	int nmbImg;
	string path;
};


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
	- in order to save memory in the example folder there is only one image which is loaded only once
		and then it is copied imagenumber-1 times;
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

	CImg<unsigned char> *image = new CImg<unsigned char>();     // modify to get all image in a folder
	image->load(imgStream[0].c_str());
	imgVector.push_back(image);
	imgDataVector.push_back(image->data());

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

	int max=0;
	int min=INT_MAX;
	workerTime.resize(nw,0);

	auto start = std::chrono::high_resolution_clock::now();

	// create the emitter node
	Emitter_farm emitter(imgDataVector,imgVector,nw);

	// create workers node
	vector<ff_node *> W;

	for (int i = 0; i < nw; ++i) {
		W.push_back(new Worker_farm(markerVec[i],i));
	}

	// crate the farm
	ff_farm<> farm;

	// add nodes to the farm
	farm.add_emitter(&emitter);
	farm.add_workers(W);

	farm.set_scheduling_ondemand();

	farm.remove_collector();

	// start farm
	if(farm.run_and_wait_end() < 0 ) error ("running Farm\n");

	auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto usec    = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();

    for (int i = 0; i < imgVector.size(); ++i)
    	delete imgVector[i];

    for (int i = 0; i < nw; ++i)
    	delete markerVec[i];
    
    
 	for (int i = 0; i < nw; ++i) {
    	if (max<workerTime[i])
    		max=workerTime[i];
    	if (min>workerTime[i])
    		min=workerTime[i];
    }

  	cout << "Parallel time: " << usec << "us, for " << imageNumber << " images, using " << nw << " workers!" << endl;
  	cout << "Worker max time: " << max << "us, for " << imageNumber << " images, using " << nw << " workers!" << endl;
  	cout << "Worker min time: " << min << "us, for " << imageNumber << " images, using " << nw << " workers!" << endl;
  	cout << "Overhead time: " << usec-max << "us, for " << imageNumber << " images, using " << nw << " workers!" << endl;

  	std::ofstream outfile;

	outfile.open("testTime.txt", std::ios_base::app);
  	outfile << nw << " " << usec << " " << max << " " << min << " " << usec-max << " " << imageNumber << endl; 

	return 0;
}