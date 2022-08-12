#include <opencv2/highgui.hpp>
#include <opencv2/video/tracking.hpp>
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


class Triangle
{
	public:
		vector<Point> contour;
		Point center;
};
class Square
{
	public:
		vector<Point> contour;
		Point center;
		vector<Triangle> triangles;
};


double angle(Point pt1, Point pt2, Point pt0) {
	double dx1 = pt1.x - pt0.x;
	double dy1 = pt1.y - pt0.y;
	double dx2 = pt2.x - pt0.x;
	double dy2 = pt2.y - pt0.y;
	return (dx1*dx2 + dy1*dy2) / sqrt((dx1*dx1 + dy1*dy1)*(dx2*dx2 + dy2*dy2) + 1e-10);
}

Mat frame;
vector<vector<Point> > contours;
vector<Point> approximace;


vector<Point2f> MarkerDetector(Mat gray)
{
	vector<Square> ctverec;

	medianBlur(gray, gray, 9);
	adaptiveThreshold(gray, gray, 255, CV_ADAPTIVE_THRESH_GAUSSIAN_C, CV_THRESH_BINARY, 15, -5);

	/*
		src – Source 8 - bit single - channel image.
		dst – Destination image of the same size and the same type as src .
		maxValue – Non - zero value assigned to the pixels for which the condition is satisfied.See the details below.
		adaptiveMethod – Adaptive thresholding algorithm to use, ADAPTIVE_THRESH_MEAN_C or ADAPTIVE_THRESH_GAUSSIAN_C.See the details below.
		thresholdType – Thresholding type that must be either THRESH_BINARY or THRESH_BINARY_INV .
		blockSize – Size of a pixel neighborhood that is used to calculate a threshold value for the pixel : 3, 5, 7, and so on.
		C – Constant subtracted from the mean or weighted mean(see the details below).Normally, it is positive but may be zero or negative as well.
	*/

	Mat drawing;
	vector<Triangle> trojuhelnik;
	vector<Point2f> markerPointy;
	vector<Vec4i> hierarchy;

	findContours(gray, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));

	for (int i = 0; i< (int)contours.size(); i++)
	{
		approxPolyDP(Mat(contours[i]), approximace, arcLength(Mat(contours[i]), true)*0.02, true);

		if (fabs(contourArea(Mat(approximace))) > 20 && isContourConvex(Mat(approximace)))
		{
			double maxCosine = 0;


			if (approximace.size() == 4)
			{
				for (int j = 2; j < 5; j++)
				{
					double cosine = fabs(angle(approximace[j % 4], approximace[j - 2], approximace[j - 1]));
					maxCosine = MAX(maxCosine, cosine);
				}
				if (maxCosine < 0.3)
				{
					Moments mu = moments(approximace, false);
					Square sq;
					sq.contour = approximace;
					sq.center = Point(mu.m10 / mu.m00, mu.m01 / mu.m00);
					ctverec.push_back(sq);
				}
			}
			if (approximace.size() == 3)
			{
				Moments mu = moments(approximace, false);
				Triangle tr;
				tr.contour = approximace;
				tr.center = Point(mu.m10 / mu.m00, mu.m01 / mu.m00);
				trojuhelnik.push_back(tr);
			}
		}
	}

	for (int i = 0; i < (int)ctverec.size(); i++)
	{
		for (int j = 0; j < (int)trojuhelnik.size(); j++)
		{
			double isIn = pointPolygonTest(ctverec[i].contour, trojuhelnik[j].center, false);
			if (isIn==1)
				ctverec[i].triangles.push_back(trojuhelnik[j]);
		}
	}


	for (int i = 0; i < (int)ctverec.size(); i++)
	{
		if (ctverec[i].triangles.size() == 2 || ctverec[i].triangles.size() == 3 || ctverec[i].triangles.size() == 4)
		{
			for (int j = 0; j < (int)ctverec[i].contour.size(); j++)
			{
				markerPointy.push_back(ctverec[i].contour[j]);
			}
			for (int k = 0; k < (int)ctverec[i].triangles.size(); k++)
			{
				for (int l = 0; l < (int)ctverec[i].triangles[k].contour.size(); l++)
				{
					markerPointy.push_back(ctverec[i].triangles[k].contour[l]);
				}
			}
		}
	}

	return markerPointy;
}


Point2f puvodniStred, stredObrazu;
static bool threadB=false;

ofstream outFile;
nlohmann::json j;

void dataLoger(int dist,float angl)
{
	ofstream log;
	string fileName="log1";
	cin >> fileName;
	log.open(fileName,std::ios::app);
	log << dist*10e-4 << " m " << angl << " degree" << endl;
	log.close();
}

float result[2];
void *SendData(void *)
{
	outFile.open("/home/pi/ramdisk/output.json");
	outFile.close();

	//BLOK PREDAVANI DAT VE FORME JSON FILU
	j["A"]=result[1];
	j["D"]=(int)result[0];
	//ZAPIS NA RAMDISK
	outFile.open("/home/pi/ramdisk/output.json");
	outFile << j;
	outFile.close();

	//MAZANI VYSTUPU KONZOLE KDE VYPISUJEM DATA
	system("clear");
	//PARSOVANI JSON FILU TAK ABYCH JE JEN VYPSAL
	cout << ((string)j.dump(4))<<endl;
	cout << j << endl;
	cout << j.size() << endl;

	/*
	//KALKULACE FPS
	usleep(1000000);
	frameR=iter;
	iter=0;
	*/
	threadB=false;
	pthread_exit(0);
}

int main()
{
	//PROMENNE PRO VYPOCTOVOU CAST
	int goodCount,diff;//iter,frameR
	bool refresh = true;
	float distance,resultx,track, positionX, traPerPx, tanAlfax;
	//PROMENNE PRO DETEKTOR
	Mat frame_prev, frame_next, gray_prev, gray_next;
	vector<Point2f> features_prev, features_next;
	VideoCapture snimekCAM;
	vector<uchar> status;
	vector<float> err;
	RotatedRect q;

	if (!snimekCAM.open(0))
		return 0;

	snimekCAM.set(CV_CAP_PROP_FPS,30);
	snimekCAM.set(CV_CAP_PROP_FRAME_WIDTH, 320);
	snimekCAM.set(CV_CAP_PROP_FRAME_HEIGHT, 240);
	//POCATECNI VYNULOVANI FRAME COUNTERU
	//iter=0;

	for (;;)
	{
		goodCount = 0;
		snimekCAM >> frame_next;
		//ITERACNI PROMENNA PRO FRAME COUNTER
		//iter++;

		if (frame_next.empty())
			break;

		frame = frame_next.clone();
		cvtColor(frame_next, gray_next, CV_BGR2GRAY);

		if (refresh)
		{
			features_next = MarkerDetector(gray_next.clone());
			if (features_next.size() != 0)
			{
				refresh = false;
			}
		}

		if (refresh)
			features_prev = features_next;

		if (frame_prev.rows > 0 && features_prev.size() > 0)
		{
			calcOpticalFlowPyrLK(gray_prev, gray_next, features_prev, features_next, status, err, Size(20, 20),5,cvTermCriteria(CV_TERMCRIT_ITER | CV_TERMCRIT_EPS,20,.3));

			for (int i = 0; i < (int)features_next.size(); i++)
			{
				if (status[i] == 1 && err[i] <= 10)
				{
					goodCount++;
					circle(frame, Point(features_prev[i].x, features_prev[i].y), 4, Scalar(255, 20, 20), 3);
					q = minAreaRect(features_prev);
					Point2f rectanglePointy[4];
					q.points(rectanglePointy);

					for(int i=0;i<4;i++)
					{
						line(frame,rectanglePointy[i],rectanglePointy[(i+1)%4],Scalar(255,255,0),3,8);
					}
				}
			}
		}

//----------------------------------------VYPOCTY-----------------------------//
		//VYPOCET VZDALENOSTI NA ZAKLADE NAMERENYCH HODNOT
		distance = ((255 * 119) / q.size.height);
		//4.8 mm je sirka CCD 1/3" a 3.67 mm je ohniskova vzdalenost webky C920
		track = (distance * 4.8) / 3.67;
		//ROZMER V XOVE OSE 320 PROTO ZE JE TO ROZLISENI OBRAZU
		traPerPx = track / frame.cols;
		//POZICE VZTAZENA KE STREDU OBRAZU
		positionX = abs(stredObrazu.x - (frame.cols / 2));
		//VYPOCET UHLU VZAVISLOSTI NA POZICI A VELIKOSTI
		tanAlfax = atan((traPerPx*positionX) / distance);
		//PREPOCET ARCTAN NA UHEL
		resultx = tanAlfax * 180 / 3.141592;

		//NASTAVENI POLARITY UHLU V OSE X KTERA STRANA JE KLADNA A KTERA ZAPORNA
		if (stredObrazu.x < (320 / 2))
		{
			resultx = (-1)*resultx;
		}

		//VYTVORENI SAMOSTATNYCH PROMENNYCH PRO STREDY A KRESLENI
		stredObrazu.x = q.center.x;
		stredObrazu.y = q.center.y;
		//VYTVORENI DIFERENCE MEZI VYSKOU A SIRKOU
		diff = abs(q.size.height - q.size.width);
		//POKUD POMER VYSKY A SIRKY V PX JE VETSI NEZ 5 ZPET NA TRACK MARKER
		if (diff >= 5)
		{
			goodCount = 0;
		}

		//KRESLENI CAR OS POZDEJI VYRADIM
		line(frame, Point(frame.size().width / 2, 0), Point(frame.size().width / 2, frame.size().height / 2), Scalar(0, 255, 0), 2, 8);
		line(frame, Point(frame.size().width / 2, frame.size().height), Point(stredObrazu.x, stredObrazu.y), Scalar(255, 0, 0), 2, 8);
		line(frame, Point(frame.size().width / 2, frame.size().height / 2), Point(frame.size().width / 2 + 80, frame.size().height / 2), Scalar(255, 0, 255), 2, 8);
		line(frame, Point(frame.size().width / 2, frame.size().height / 2), Point(stredObrazu.x, stredObrazu.y), Scalar(255, 255, 255), 2, 8);

		//TVORBA THREADU KDE SE ODESILAJI DATA DO RAMDISKU
		if(!threadB)
		{
			//UKLADANI VZDALENOSTI V MM
			result[0]=distance;
			//UKLADANI UHLU VE STUPNICH
			result[1]=resultx;
			//POKUD THREADB JE TRUE TAK NETVORIME NOVEJ THREAD
			threadB=true;
			//VYTVORENI THREADU
			pthread_t t1;
			pthread_create(&t1,NULL,SendData,NULL);
			//THREAD SE UKONCI ALE HLAVNI NA NEJ NEMUSI CEKAT
			pthread_detach(t1);
		}


		//VYPSANI AKTULANICH FPS PROGRAMU
		//cout << frameR;
		//VYKRESLENI FRAMU S OSOU
		imshow("frame", frame);
		//PRENOS DALSICH FRAMU DO DALSI ITERACE
		frame_prev = frame_next.clone();
		gray_prev = gray_next.clone();


		if (!refresh)
			features_prev = features_next;
		if (goodCount < 3)
		{
			refresh = true;
		}
		int key = waitKey(10);

		if (key == 32)
		{
			refresh = false;
		}
		if (key == 27)
		{
			refresh = true;
		}
	}
	return 0;
}


