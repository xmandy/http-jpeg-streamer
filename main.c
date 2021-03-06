#include "server.h"

frame frame_buffer[FRAME_COUNT];
pthread_mutex_t lock_buf = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char **argv)
{
	pthread_t server;

	char file[] = "bmp/shot-%03d.bmp";
	char name[20];
	int err;
	int i;
	int nameloop;
	// Variables for image.
	FILE *input_file;
	unsigned char *imagedata;
	unsigned char * rowdata;
	long width;
	long height;
	long row_width;

	// Variables for libjpg.
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW row_pointer;






	// first we initialize the global frame buffer.
	init_global();	
	//pthreadid("main");

	// here we create a thread to listen client listen.
	if(pthread_create(&server, NULL, &accept_thread, NULL)) {
		err_sys("accept_thread create:");
	}
	pthread_detach(server);

	// in main thread, we compress images and send them if there is a client.

			
	i = 0;
	nameloop = 0;
	struct timeval start;
	struct timeval end;
	int elapse;
	int sleepnu;

	while(1) {
		if((err = gettimeofday(&start, NULL)) != 0)
			err_sys("gettimeofday");
		i = i % FRAME_COUNT;
		if(i == 0) 
			DBG("start:%d\n", time(NULL));

		nameloop = nameloop % 375;

		memset(name, 0, 20);
		sprintf(name, file, nameloop + 1);
		input_file = NULL;
		input_file = fopen(name, "rb");
		if(!input_file) {
			exit_global();
			err_sys("open file %s", name);
		}

		
		imagedata = process_bmpfile(input_file, &height, &width);
		if(!imagedata) {
			exit_global();
			err_sys("process file %s:", name);
		}
		row_width = width*3;
		// align the width at 4 byte.
		while((row_width & 3) != 0) row_width++;

		cinfo.err = jpeg_std_error(&jerr);
		jpeg_create_compress(&cinfo);

	//	DBG("begin compress frame:%d\n", i);
		pthread_mutex_lock(&(frame_buffer[i].lock));
	//	DBG("lock succeed\n");

		memset(frame_buffer[i].data, 0, FRAME_BUFFER_SIZE);

	//     see jdatadst.c
		my_jpeg_stdio_dest(&cinfo, frame_buffer[i].data, &frame_buffer[i].length);

		cinfo.image_width = width;
		cinfo.image_height = height;
		cinfo.input_components = 3;
		cinfo.in_color_space = JCS_RGB;

		jpeg_set_defaults(&cinfo);
		jpeg_set_quality(&cinfo, 75, TRUE);
		jpeg_start_compress(&cinfo, TRUE);

		while(cinfo.next_scanline < cinfo.image_height) {
			rowdata = get_24bit_row(imagedata, cinfo.next_scanline, height, row_width, width);
			row_pointer = (JSAMPROW) rowdata;
			(void) jpeg_write_scanlines(&cinfo, &row_pointer, 1);
			free(rowdata);
		}
		
		jpeg_finish_compress(&cinfo);
		
		// set the compress complete time.
		gettimeofday(&(frame_buffer[i].time), NULL);

		pthread_mutex_unlock(&(frame_buffer[i].lock));
	//	DBG("complete compress frame:%d\n", i);


		fclose(input_file);
		free(imagedata);
		jpeg_destroy_compress(&cinfo);

		i++;
		nameloop ++;
		if((err = gettimeofday(&end, NULL)) != 0)
			err_sys("gettimeofday");
		elapse = get_elapse(start, end);
		sleepnu = FRAME_USECS - elapse;
		//DBG("sleepnu:%d\n", sleepnu);
		if(sleepnu > 0)
			usleep(sleepnu);
	}
	
	exit_global();

}
