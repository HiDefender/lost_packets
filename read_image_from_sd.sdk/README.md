****************This the red tint implementation for FPGA****************
1. This folder inclued the exported hardware, standalone BSP, and C project
2. The red tint code is in \xliff_example\src\redTint.c
3. To change the batch size, please change the value of BATCH_SIZE
4. Image name formation:
    (a) Input image has to be 32bit bitmap image
	(b) Name of the input image has to start with 1.bmp
	(c) The following image names have to be 2.bmp, 3.bmp, ...
5. The result image will be generated and saved in the SD card
6. Generated image names are x_RED.BMP.
