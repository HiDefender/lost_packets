
/***************************** Include Files *********************************/
#include <stdio.h>
#include <stdlib.h>
#include "xparameters.h"	/* SDK generated parameters */
#include "xsdps.h"		/* SD device driver */
#include "ff.h"
#include "xil_cache.h"
#include "xplatform_info.h"
#include "xtime_l.h"

/************************** Constant Definitions *****************************/
#define BITMAPFILEHEADERLENGTH 14   // The bmp FileHeader length is 14
#define BM 19778                    // The ASCII code for BM
#define BATCH_SIZE 5
/**************************** Type Definitions *******************************/

/************************** Function Prototypes ******************************/
unsigned int* readImage(const char *filename, int* widthOut, int* heightOut);
void applyRedTint(UINT* img, int height, int width);
void writeImage(UINT *imageOut, const char *filename, int rows, int cols, const char* refFilename);
void runAll(const char *newfilename, const char *refFilename);
/************************** Variable Definitions *****************************/
static FIL fil;		/* File object */
static FIL ofil;
static FATFS fatfs;
static UINT* img;

XTime tStart, tEnd;
XTime tStart_par, tEnd_par;
double total_batch_time = 0;
double total_redT_time = 0;
u64 total_batch_cycle = 0;
u64 total_redT_cycle = 0;

u32 Platform;

#ifdef __ICCARM__
#pragma data_alignment = 32
u8 DestinationAddress[10*1024*1024];
u8 SourceAddress[10*1024*1024];
#pragma data_alignment = 4
#else
u8 DestinationAddress[10*1024*1024] __attribute__ ((aligned(32)));
u8 SourceAddress[10*1024*1024] __attribute__ ((aligned(32)));
#endif

int main(void)
{
	print("platform initial.\n");
	init_platform();

//	int w, h;
//	UINT* img = readImage("Lenna.bmp", &h, &w);
//	applyRedTint(img, h, w);
//	writeImage(img, "red_Lenna.bmp", h, w, "Lenna.bmp");

	const char* strExtention = ".bmp";
	const char* afterwardStrExtention = "_red.bmp";

	//loop batch
	for(int i = 1; i <= BATCH_SIZE; i++) {

		int tmp = BATCH_SIZE;
		int counter = 0;
		while (tmp != 0) {
			counter++;
			tmp = tmp / 10;
		}

		char Fname[counter + 1];
		sprintf(Fname, "%d", i);

		char refFullName[counter + 5];
		strcpy(refFullName, Fname);
		strcat(refFullName, strExtention);

		char newFullName[counter + 9];
		strcpy(newFullName, Fname);
		strcat(newFullName, afterwardStrExtention);

		XTime_GetTime(&tStart);
		runAll(newFullName,refFullName);
		XTime_GetTime(&tEnd);


		printf("Red tint process took %llu clock cycles.\n", 2*(tEnd_par - tStart_par));
		printf("Red tint process took %.2f us.\n",
			   1.0 * (tEnd_par - tStart_par) / (COUNTS_PER_SECOND/1000000));

		total_redT_cycle += 2*(tEnd_par - tStart_par);
		total_redT_time += 1.0 * (tEnd_par - tStart_par) / (COUNTS_PER_SECOND/1000000);

		printf("Whole process took %llu clock cycles.\n", 2*(tEnd - tStart));
		printf("Whole process took %.2f us.\n",
			   1.0 * (tEnd - tStart) / (COUNTS_PER_SECOND/1000000));

		total_batch_cycle = 2*(tEnd - tStart);
		total_batch_time = 1.0 * (tEnd - tStart) / (COUNTS_PER_SECOND/1000000);
	}
	printf("Red tint process took %llu clock cycles totally.\n", total_redT_cycle);
	printf("Red tint process took %.2f us totally.\n", total_redT_time);

	printf("Whole process took %llu clock cycles totally.\n", total_batch_cycle);
	printf("Whole process took %.2f us totally.\n", total_batch_time);

	printf("One red tint process took %llu clock cycles averagely.\n", total_redT_cycle / BATCH_SIZE);
	printf("One red tint process took %.2f us averagely.\n", total_redT_time / BATCH_SIZE);

	printf("Whole process took %llu clock cycles averagely.\n", total_batch_cycle / BATCH_SIZE);
	printf("Whole process took %.2f us averagely.\n", total_batch_time / BATCH_SIZE);

	cleanup_platform();
	return XST_SUCCESS;

}

unsigned int* readImage(const char *filename, int* heightOut, int* widthOut) {

	int height, width;
	int offset;

	UINT bw;

	/*
	*  Write files to SDC using FatFs SPI protocol
	*/
	// mount SD card
	f_mount(&fatfs, "0:/", 0); /* Give a work area to the default drive */

	printf("Reading input image from %s\n", filename);

	int state = f_open(&fil, filename, FA_READ);
	if (state != FR_OK) {
		return NULL;
	}

	f_lseek(&fil, 10);
	f_read(&fil, &offset, 4, &bw);

	f_lseek(&fil, 18);
	f_read(&fil, &width, 4, &bw);
	f_read(&fil, &height, 4, &bw);

	unsigned int pix;
	img = (UINT *)malloc(sizeof(UINT)*height*width);

	//Read pixels of image
	f_lseek(&fil, offset);
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			f_read(&fil, &pix, 4, &bw);
			img[i*width+j] = pix;
		}
	}
	*widthOut = width;
	*heightOut = height;
	printf("Reading finished.\n");
	return img;
}

void applyRedTint(UINT* img, int height, int width) {
	printf("Applying red tint.\n");
	printf("pix: %u\n", *(img+width));
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			UINT pix = *(img+i*width+j);
			//separate channels
			u8 red = pix >> 16;
			u8 green = (pix >> 8) - (red << 8);
			u8 blue = pix - (green << 8) - (red << 16);
			//applying red tint
			if (red < 128) {
				red = red * 2;
			} else {
				red = 255;
			}
			UINT tmp = (red << 16) + (green << 8) + blue;
			*(img+i*width+j) = tmp;
		}
	}
	printf("Red tint finished.\n");
}

void writeImage(UINT *imageOut, const char *filename, int rows, int cols, const char* refFilename) {

	printf("Writing image to %s\n", refFilename);

	UINT pix;
	int offset;
	unsigned char *buffer;
	int height, width;

	UINT bw; // bytes written

	/*
	*  Write files to SDC using FatFs SPI protocol
	*/

	f_mount(&fatfs, "0:/", 0); /* Give a work area to the default drive */

	int state = f_open(&fil, refFilename, FA_READ);
	if (state != FR_OK) {
		return;
	}

	f_lseek(&fil, 10);
	f_read(&fil, &offset, 4, &bw);

	f_lseek(&fil, 18);
	f_read(&fil, &width, 4, &bw);
	f_read(&fil, &height, 4, &bw);
	f_lseek(&fil, 0);

	buffer = (unsigned char *)malloc(offset);
	if (buffer == NULL) {
		return;
	}
	f_read(&fil, buffer, offset, &bw);


	state = f_open(&ofil, filename, FA_WRITE | FA_CREATE_ALWAYS);
	if (state != FR_OK) {
		return;
	}

	f_write(&ofil, buffer, offset, &bw);
	if (bw != offset) {
		return;
	}

	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			pix = imageOut[i*cols + j];
			f_write(&ofil, &pix, sizeof(UINT), &bw);
		}
	}

	f_close(&ofil);
	f_close(&fil);

	free(buffer);
	printf("Writing finished.\n");
}

void runAll(const char *newfilename, const char *reffilename) {

	int height, width;
	int offset;

	UINT bw;

	/*
	*  Write files to SDC using FatFs SPI protocol
	*/
	// mount SD card
	f_mount(&fatfs, "0:/", 0); /* Give a work area to the default drive */

	printf("Reading input image from %s\n", reffilename);

	int state = f_open(&fil, reffilename, FA_READ);
	if (state != FR_OK) {
		return;
	}
	int ostate = f_open(&ofil, newfilename, FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
	if (ostate != FR_OK) {
		return;
	}

	f_lseek(&fil, 10);
	f_read(&fil, &offset, 4, &bw);

	f_lseek(&fil, 0);

	unsigned char buffer[offset];
	if (buffer == NULL) {
		return;
	}
	f_read(&fil, buffer, offset, &bw);

	f_lseek(&fil, 18);
	f_read(&fil, &width, 4, &bw);
	f_read(&fil, &height, 4, &bw);

	unsigned int pix;
	UINT img[height*width];

	//Read pixels of image
	f_lseek(&fil, offset);
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			f_read(&fil, &pix, 4, &bw);
			img[i*width+j] = pix;
		}
	}
	printf("Reading finished.\n");

	printf("Applying red tint.\n");
	XTime_GetTime(&tStart_par);
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			UINT pix = img[i*width+j];
			//separate channels
			u8 red = pix >> 16;
			u8 green = (pix >> 8) - (red << 8);
			u8 blue = pix - (green << 8) - (red << 16);
			//applying red tint
			if (red < 128) {
				red = red * 2;
			} else {
				red = 255;
			}
			UINT tmp = (red << 16) + (green << 8) + blue;
			img[i*width+j] = tmp;
		}
	}
	XTime_GetTime(&tEnd_par);
	printf("Red tint finished.\n");

	printf("Writing image to %s\n", newfilename);

	f_write(&ofil, buffer, offset, &bw);
	if (bw != offset) {
		return;
	}

	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			pix = img[i*width + j];
			f_write(&ofil, &pix, sizeof(UINT), &bw);
		}
	}

	f_close(&ofil);
	f_close(&fil);
	printf("Writing finished.\n");
}
