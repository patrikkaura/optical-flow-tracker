#include <opencv2/highgui/highgui.hpp>
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/objdetect/objdetect.hpp"
#include <iostream>
#include <random>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/core/core.hpp>  
#include <opencv2/video/background_segm.hpp>
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/video/video.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <cmath>



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
class Result
{
public:
	int timeSquare;
	int prevTimeSquare;

	Square prevSquare;
	Square square;

	Point pAprev;
	Point pBprev;
	Point pCprev;
	Point pDprev;

	Point pA;
	Point pB;
	Point pC;
	Point pD;
};
//----------------------------------------------------------------------------------------------------
double angle(Point pt1, Point pt2, Point pt0)
{
	//SKALARNI SOUCIN MEZI VEKTORY VYYSLEDKEM JE ARCOS TZN UHEL MEZI VEKTORY PROTO SESTAVUJU DVA VEKTORY 1 A 2 
	double dx1 = pt1.x - pt0.x;
	double dy1 = pt1.y - pt0.y;
	double dx2 = pt2.x - pt0.x;
	double dy2 = pt2.y - pt0.y;
	return (dx1*dx2 + dy1*dy2) / sqrt((dx1*dx1 + dy1*dy1)*(dx2*dx2 + dy2*dy2) + 1e-10);
}
Mat frame;



vector<Point2f> MarkerDetector(Mat gr){
	
	Result vysledek;
	medianBlur(gr, gr,9);//ZMENA??? 9->11
	adaptiveThreshold(gr, gr, 255, CV_ADAPTIVE_THRESH_GAUSSIAN_C, CV_THRESH_BINARY, 15,-10);
	imshow("thresh", gr);

	/*
	src – Source 8 - bit single - channel image.
	dst – Destination image of the same size and the same type as src .
	maxValue – Non - zero value assigned to the pixels for which the condition is satisfied.See the details below.
	adaptiveMethod – Adaptive thresholding algorithm to use, ADAPTIVE_THRESH_MEAN_C or ADAPTIVE_THRESH_GAUSSIAN_C.See the details below.
	thresholdType – Thresholding type that must be either THRESH_BINARY or THRESH_BINARY_INV .
	blockSize – Size of a pixel neighborhood that is used to calculate a threshold value for the pixel : 3, 5, 7, and so on.
	C – Constant subtracted from the mean or weighted mean(see the details below).Normally, it is positive but may be zero or negative as well.
	*/

	vector<vector<Point>> kontury;	//kazda kontura je ulozena jako vektor bodu
	vector<Vec4i> hiearchie;//hiearchie uklada vzajemnou topologii kontur tzn graf

	findContours(gr, kontury, hiearchie, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));

	//ZOBRAZENI KONTUR draw funkce
	Mat drawingKont = Mat::zeros(gr.size(), CV_8UC3);//vytvoreni nove matice o stejne velikosti jako gr a plne nul
	vector<Square> ctverce; //zasobnik na ctverce
	vector<Triangle> trojuhelniky; //zasobnik na trojuhelniky
	vector<Point> aproximace;//aproximovane kontury
	vector<Point2f> markerPointy;//VYSLEDNE BODY DO KTERYCH ZANASIME PRECHODY KTERE ZOBRAZUJEME

	for (int i = 0; i < kontury.size(); i++)//pruchod vektory kontur
	{

		approxPolyDP(Mat(kontury[i]), aproximace, arcLength(Mat(kontury[i]), true)*0.02, true);
		//aproximacni polynom veme kontury pak je narve do aproximace true= musi byt uzavrene
		//arclength*0.02 je cislo epsilon udavajici presnost aproximace

		//pokud je plocha kontury vetsi nez 50 a je konvexni  

		//OTAZKA 1 VYPOCET UHLU PROC A JAK?? 
		if (fabs(contourArea(Mat(aproximace)))>50 && isContourConvex(Mat(aproximace)))
		{
			double maxCOSIN = 0;
			if (aproximace.size() == 4) //TZN CTYRI STRANY
			{
				for (int j = 2; j < 5; j++)
				{
					double cosin = fabs(angle(aproximace[j % 4], aproximace[j - 2], aproximace[j - 1]));
					maxCOSIN = MAX(maxCOSIN, cosin);
				}
				if (maxCOSIN < 0.3)
				{
					Square SQ;
					SQ.contour = aproximace; //do ctverce nahraju jeho konturu ale jiz aproximovanou
					Moments moment;
					moment = moments(aproximace, false);
					//false protoze vsechny nenulove pixeli nechci povazovat za 1
					SQ.center = Point(moment.m10 / moment.m00, moment.m01 / moment.m00);
					/*
						// spatial moments
						double  m00, m10, m01, m20, m11, m02, m30, m21, m12, m03;
						// central moments
						double  mu20, mu11, mu02, mu30, mu21, mu12, mu03;
						// central normalized moments
						double  nu20, nu11, nu02, nu30, nu21, nu12, nu03;
						*/
					ctverce.push_back(SQ);
				}
			}
			if (aproximace.size() == 3)//POKUD MA TRI STRANY
			{
				Triangle TR;
				TR.contour = aproximace;
				Moments moment;
				moment = moments(aproximace, false);
				TR.center = Point(moment.m10 / moment.m00, moment.m01 / moment.m00);
				trojuhelniky.push_back(TR);//narvu zpet do struktury trojuhelniky PRIDANI NOVE KONTURY NA KONEC VEKTORU
			}
		}
	}
	for (int i = 0; i < ctverce.size(); i++)//prochazim celej vektor kde jsou ulozeny vsechny kontury ctvercu
	{
		for (int j = 0; j < trojuhelniky.size(); j++)
		{
			//prochazim tedy vsechny ctverce co nasel a vsechny trojuhelniky ktere jsou uvnitr tech ctvercu
			double isIN = pointPolygonTest(ctverce[i].contour, trojuhelniky[j].center, false);//false nechci vracet vzdalenost teziste troj. a okraje ctverce
			if (isIN == 1)
			{
				ctverce[i].triangles.push_back(trojuhelniky[j]);
				//pro kazdej ctverec kterej obsahuje trojuhelnik uvnitr tak ho doplnim do vektoru 
				//takze mam vektor s aprox konturou ctverce a trojuhelniku
			}
		}
	}

	//PRUCHOD VSEMI CTVERCI VNICHZ JSOU UZ TROJUHELNIKY NAHRANY

	vector<vector<Point>> Markers; //VEKTOR VEKTORU BODU

	for (int i = 0; i < ctverce.size(); i++)
	{
		//PROC 2 3 4 ????  
		if (ctverce[i].triangles.size() == 2 || ctverce[i].triangles.size() == 3 || ctverce[i].triangles.size() == 4)
		{
			Markers.push_back(ctverce[i].contour);
			vysledek.square = ctverce[i];

			for (int j = 0; j < ctverce[i].contour.size(); j++)
			{
				markerPointy.push_back(ctverce[i].contour[j]); //ZANESENI VNEJSICH OKRAJU DO VYSLEDNEHO VEKTORU
				circle(frame, ctverce[i].contour[j], 8, Scalar(0, 255, 255), 8); //ZLUTE BODY TZN VNEJSI OKRAJ DETEKCE
				//circle(frame, ctverce[i].center, 1, Scalar(255, 255, 255), 8);
				
				putText(frame, to_string(ctverce[i].center.x), Point(0, 100), 1, 2, CV_RGB(255, 255, 0), 2, 2, false);
				putText(frame, to_string(ctverce[i].center.y), Point(0, 150), 1, 2, CV_RGB(255, 255, 0), 2, 2, false);
			}
			for (int k = 0; k < ctverce[i].triangles.size(); k++)
			{
				Markers.push_back(ctverce[i].triangles[k].contour);

				for (int l = 0; l < ctverce[i].triangles[k].contour.size(); l++)
				{
					markerPointy.push_back(ctverce[i].triangles[k].contour[l]);//DETEKOVANI VNITRNICH BODU ALE ???? 
					circle(frame, ctverce[i].triangles[k].contour[l], 8, Scalar(255, 0, 0), 8);
				}
			}
		}
	}

	



	drawContours(frame, Markers, -1, Scalar(0, 255, 17), 5);
	return markerPointy;

}




int main(int argc, char** argv)
{
	
	vector<Point2f> features_previous, features_next;
	Mat frame_previous, frame_next, gray_previous, gray_next;

	VideoCapture snimekCAM;



	if (!snimekCAM.open(1)) //pokud se DEFAULT 0 INTERNI nebo EXTERN 1 neotevre zavre aplikaci 
		return 0;

	bool refresh = true;

	//snimekCAM.set(CV_CAP_PROP_FRAME_WIDTH, 320);
	//snimekCAM.set(CV_CAP_PROP_FRAME_HEIGHT, 240);

	while (true)
	{
		vector<Point> box;
		int goodCount = 0;

		snimekCAM >> frame_next;
		if (frame_next.empty()) 
			break;
		frame = frame_next.clone();
		cvtColor(frame_next, gray_next, CV_BGR2GRAY);
		
		if (refresh)
		{
			features_next = MarkerDetector(gray_next);
			if (features_next.size() != 0)
				refresh = true;
		}
		imshow("obraz", frame);

		
		if (waitKey(1) == 32) break; // waitKey(int delay=0) 27=ESC klavesa 32=mezernik

		
		
	}




	return 0;
}