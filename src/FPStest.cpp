#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <cmath>
#include <time.h>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <pthread.h>
#include <unistd.h>
#include <string>
#include "json.hpp"
#include <time.h>

using namespace cv;
using namespace std;

Mat frame, hranyCanny, frame_grey, frame_blur, image;
VideoCapture snimekCAM;
static bool threadB=false;
static int iter,frameR;

void *drawSend(void *)
{
	usleep(1000000);

	frameR=iter;
	iter=0;

	std::system("clear");

	threadB=false;
	pthread_exit(0);
}

int main(int argc, char** argv)
{



	if (!snimekCAM.open(0)) //pokud se DEFAULT 0 INTERNI nebo EXTERN 1 neotevre zavre aplikaci
		return 0;

	snimekCAM.set(CV_CAP_PROP_FRAME_WIDTH, 320);
	snimekCAM.set(CV_CAP_PROP_FRAME_HEIGHT, 240);
	snimekCAM.set(CV_CAP_PROP_GAIN,1);
	//snimekCAM.set(CV_CAP_PROP_CONVERT_RGB, false);



	while (true)
	{

		snimekCAM >> frame;
		iter++;
		if(!threadB)
		{
			threadB=true;
			pthread_t t1;
			pthread_create(&t1,NULL,drawSend,NULL);
			pthread_detach(t1);
		}
		cout << frameR << endl;

		if (frame.empty()) break; //pokud nedojde nic konci cyklus
		imshow("Fram", frame);
		if (waitKey(1) == 32) break; // waitKey(int delay=0) 27=ESC klavesa 32=mezernik
	}

	return 0;
}
