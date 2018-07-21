#include <iostream>
#include <string>
#include <vector>
#define cimg_use_jpeg 1
#include "CImg.h"


using namespace std;
using namespace cimg_library;


int main(int argc, char const *argv[])
{

	int nw = atoi(argv[1]);
	string markName = argv[2];

	CImg<unsigned char> mark;
	unsigned char *markVec;

	mark.load(markName.c_str());

	markVec = mark.data();

	vector<int> infoVector;
	vector<int> blackVec;

	int numBlack=0;

	int markHeight = mark.height();
	int markWidth = mark.width();

	int imgNmbPx ,chunksRow ,lastRow;

	imgNmbPx = markHeight*markWidth;

	chunksRow = (markHeight/nw)+1;
	lastRow = markHeight%nw;

	// loop on workers
	for ( int i=0 ; i<nw ; i++ ) {

		// last row say me how many row exceed from the split, when i assigned all of it i decrease the number of row to give to worker
		if ( lastRow == i )
			chunksRow=chunksRow-1;

		infoVector.push_back(chunksRow);

	}

	int rows=0;
	int rowTarget=infoVector[0];
	int index=0;

	for (int y = 0; y < markHeight; ++y)
	{
		for (int x = 0; x < markWidth ; ++x)
		{
			if (mark(x,y)<50)
				numBlack++;
		
		}

		if (y==rowTarget-1) {
			cout << "Black= " << numBlack << endl;
			blackVec.push_back(numBlack);
			numBlack=0;
			if (index<nw-1) {
				index++;
				rowTarget+=infoVector[index];
			}
		}

	}

	int max=0;
	int min=10000;

	for (int i = 0; i < nw; ++i)
	{
		if (blackVec[i]>max)
			max=blackVec[i];
		if (blackVec[i]<min)
			min=blackVec[i];
	}

	cout << "max " << max << " min " << min << endl;


	return 0;
}