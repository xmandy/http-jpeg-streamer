#include "server.h"

int read_byte(FILE *input_file)
{
		int c;
		if((c = getc(input_file)) == EOF){
				printf("file has reached end!\n");
				return -1;
		}
		return c;
}

unsigned char* get_24bit_row(unsigned char *imagedata, long row_number, long height, long row_width, long width)
{
	// here we reverse the image data row and rgb secquence.
	long row_position;
	unsigned char *row_pointer;
	unsigned char *tempptr;
	unsigned char * rowptr;
	unsigned char *first;
	unsigned int debugR, debugG, debugB;
	long col;

	row_position = height - 1 - row_number; // Since next_scanline starts from 0.
	row_pointer = imagedata + row_width*row_position;

	// allocate one row buffer to store the requseted image row data.
	rowptr = (unsigned char*)malloc(sizeof(unsigned char)*width*3);
	if(!rowptr) {
			printf("allocate row buffer failed!\n");
			return NULL;
	}

	tempptr = rowptr;
	first = row_pointer;
	for(col = width; col > 0; col--) {
		tempptr[2] = *first++;
		tempptr[1] = *first++; 
		tempptr[0] = *first++;
		debugR = (unsigned int) tempptr[0]; 
		debugG = (unsigned int) tempptr[1]; 
		debugB = (unsigned int) tempptr[2]; 
		tempptr += 3;	
	}
	return rowptr;
}



unsigned char* process_bmpfile(FILE *input_file, long *height, long *width)
{
	unsigned char bmpfileheader[14];
	unsigned char bmpinfoheader[64];
	long bfOffBits;
	long headerSize;
	long biWidth;
	long biHeight;
	unsigned int biPlanes;
	long biCompression;
	long biXPelsPerMeter, biYPelsPerMeter;
	long biClrUsed = 0;
	long bPad;
	long row_width; // padding to 4-byte boundary.
	long biDataSize;
	unsigned char *image;

	if(fread(bmpfileheader,1,14,input_file) != 14) {
		printf("read file header error!\n");
		return NULL;
	}
	if(GET_2B(bmpfileheader, 0) != 0x4D42) {
			printf("file is not bmp format!\n");
			return NULL;
	}

	bfOffBits = (long) GET_4B(bmpfileheader,10);
	
	if(fread(bmpinfoheader, 1, 4, input_file) != 4) {
			printf("read info header size error!\n");
			return NULL;
	}

	headerSize = (long)GET_4B(bmpinfoheader, 0);
	if(headerSize < 12 || headerSize > 64) {
			printf("file info header is not bmp format!\n");
			return NULL;
	}
	
	if(fread(bmpinfoheader+4, 1, headerSize-4, input_file) != headerSize-4) {
			printf("read info header error!\n");
			return NULL;
	}

	// ingnore the format we don't use, so assume the headerSize is 40.
	// ingnore the file with colormap, because we don't use this format image.
	// assume the bits_per_pixel is 24, because we don't use others.
	
	biWidth = GET_4B(bmpinfoheader, 4);
	biHeight = GET_4B(bmpinfoheader, 8);
	biPlanes = GET_2B(bmpinfoheader, 12);
	if((int) GET_2B(bmpinfoheader, 14) != 24) {
			printf("file is not 24 bit per pixel!\n");
			return NULL;
	}
	biCompression = GET_4B(bmpinfoheader, 16);
	biDataSize = GET_4B(bmpinfoheader, 20);
	if(biCompression != 0) {
			printf("file is a compressed bmp file!\n");
			return NULL;
	}
	if(biWidth <= 0 || biHeight <= 0) {
			printf("file is an empty image!\n");
			return NULL;
	}
	if(biPlanes != 1) {
			printf("error planes!\n");
			return NULL;
	}

	

	bPad = bfOffBits - (headerSize + 14);

	// ignore the color map, just to the data copy.
	while(--bPad >= 0) {
			read_byte(input_file);
	}

	// compute row width in file, including to 4-byte boundary
	row_width = biWidth*3;
	while((row_width &3) != 0) row_width ++;


	// allocat memory to store image data.
	image = (unsigned char *) malloc(sizeof(unsigned char)*row_width*biHeight);
	if(!image) {
		printf("memory alloc failed!\n");
		return NULL;
	}

	printf("image_size:%d\n", sizeof(image));

	if(fread(image, 1, sizeof(unsigned char)*row_width*biHeight, input_file) != sizeof(unsigned char)*row_width*biHeight) {
			printf("read image data error!\n");
			free(image);
			return NULL;
	}

	*width = biWidth;
	*height = biHeight;
	return image;	
}


