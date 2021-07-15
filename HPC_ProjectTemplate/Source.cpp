#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <mpi.h>
#include<string.h>
#include<msclr\marshal_cppstd.h>
#include <ctime>// include this header 
#pragma once

#using <mscorlib.dll>
#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>
using namespace std;
using namespace msclr::interop;

int* inputImage(int* w, int* h, System::String^ imagePath) //put the size of image in w & h
{
	int* input;


	int OriginalImageWidth, OriginalImageHeight;

	//********************Read Image and save it to local arrayss********	
	//Read Image and save it to local arrayss

	System::Drawing::Bitmap BM(imagePath);

	OriginalImageWidth = BM.Width;
	OriginalImageHeight = BM.Height;
	*w = BM.Width;
	*h = BM.Height;
	int* Red = new int[BM.Height * BM.Width];
	int* Green = new int[BM.Height * BM.Width];
	int* Blue = new int[BM.Height * BM.Width];
	input = new int[BM.Height * BM.Width];
	for (int i = 0; i < BM.Height; i++)
	{
		for (int j = 0; j < BM.Width; j++)
		{
			System::Drawing::Color c = BM.GetPixel(j, i);

			Red[i * BM.Width + j] = c.R;
			Blue[i * BM.Width + j] = c.B;
			Green[i * BM.Width + j] = c.G;

			input[i * BM.Width + j] = ((c.R + c.B + c.G) / 3); //gray scale value equals the average of RGB values

		}

	}
	return input;
}


void createImage(int* image, int width, int height, int index)
{
	System::Drawing::Bitmap MyNewImage(width, height);


	for (int i = 0; i < MyNewImage.Height; i++)
	{
		for (int j = 0; j < MyNewImage.Width; j++)
		{
			//i * OriginalImageWidth + j
			if (image[i * width + j] < 0)
			{
				image[i * width + j] = 0;
			}
			if (image[i * width + j] > 255)
			{
				image[i * width + j] = 255;
			}
			System::Drawing::Color c = System::Drawing::Color::FromArgb(image[i * MyNewImage.Width + j], image[i * MyNewImage.Width + j], image[i * MyNewImage.Width + j]);
			MyNewImage.SetPixel(j, i, c);
		}
	}
	MyNewImage.Save("..//Data//Output//outputRes" + index + ".png");
	cout << "result Image Saved " << index << endl;
}



int main()
{
	int ImageWidth = 4, ImageHeight = 4;

	int start_s, stop_s, TotalTime = 0;

	System::String^ imagePath;
	std::string img;
	img = "..//Data//Input//test.png";

	imagePath = marshal_as<System::String^>(img);
	int* imageData = inputImage(&ImageWidth, &ImageHeight, imagePath);

	start_s = clock();
	/**/
	MPI_Init(NULL, NULL);
	int dim = ImageHeight * ImageWidth;
	int size, rank;
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	int part = dim / size;
	int rem = dim % size;
	int arr[256] = { 0 };
	int* localpart = (int*)malloc(sizeof(int) * part);
	int* ptrImage = NULL;
	if (rank == 0) {
		ptrImage = imageData;
	}
	//frequency of each intensity
	MPI_Scatter(ptrImage, part, MPI_INT, localpart, part, MPI_INT, 0, MPI_COMM_WORLD);
	for (int i = 0; i < part; i++) {
		arr[localpart[i]]++;
	}
	//element wise summation 
	if (rank == 0)
		MPI_Reduce(MPI_IN_PLACE, arr, 256, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	else
		MPI_Reduce(arr, nullptr, 256, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	double* finalsum = NULL;
	if (rank == 0 && rem != 0) {
		for (int i = (part * size); i < dim; i++) {
			arr[ptrImage[i]]++;
		}
	}
	//Calculating the Cummulative Probability 
	double* Prob = NULL;
	if (rank == 0) {
		Prob = new double[256];
		for (int i = 0; i < 256; i++) {
			Prob[i] = (double)arr[i] / dim;
		}
		for (int i = 1; i < 256; i++) {
			Prob[i] = Prob[i] + Prob[i - 1];
		}
		for (int i = 0; i < 256; i++) {
			arr[i] = Prob[i] * 255;
		}
	}

	MPI_Bcast(arr, 256, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Status stat;
	//replacing the old intensity with the new ones
	if (rank == 0) {
		for (int i = 1; i < size; i++) {
			MPI_Send(ptrImage + (i * part), part, MPI_INT, i, i, MPI_COMM_WORLD);
		}
		for (int i = 0; i < part; i++) {
			ptrImage[i] = arr[ptrImage[i]];
		}
		for (int i = (size * part); i < dim; i++) {
			ptrImage[i] = arr[ptrImage[i]];
		}
		for (int i = 1; i < size; i++) {
			MPI_Recv(ptrImage + (i * part), part, MPI_INT, i, i, MPI_COMM_WORLD, &stat);
		}
	}
	else {
		MPI_Recv(localpart, part, MPI_INT, 0, rank, MPI_COMM_WORLD, &stat);
		for (int i = 0; i < part; i++) {
			localpart[i] = arr[localpart[i]];
		}
		MPI_Send(localpart, part, MPI_INT, 0, rank, MPI_COMM_WORLD);
	}
	MPI_Finalize();
	/**/
	if (rank == 0) {
		createImage(ptrImage, ImageWidth, ImageHeight, rank);
		stop_s = clock();
		TotalTime += (stop_s - start_s) / double(CLOCKS_PER_SEC) * 1000;
		cout << "time: " << TotalTime << endl;
	}
	free(imageData);
	return 0;

}


//// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
//// Debug program: F5 or Debug > Start Debugging menu
//
//// Tips for Getting Started: 
////   1. Use the Solution Explorer window to add/manage files
////   2. Use the Team Explorer window to connect to source control
////   3. Use the Output window to see build output and other messages
////   4. Use the Error List window to view errors
////   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
////   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
//
//
