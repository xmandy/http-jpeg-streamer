#include "server.h"

frame frame_buffer[FRAME_COUNT];

struct bufferevent *bufev;

int main(int argc, char **argv)
{
	pthread_t server;

	char file[] = "shot-%03d.bmp";
	char name[20];
	int err;
	int i;
	// Variables for image.
	FILE *input_file, *output_file;
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
	pthreadid("main");

	// here we create a thread to listen client listen.
	if(pthread_create(&server, NULL, &accept_thread, NULL)) {
		err_sys("accept_thread create:");
	}
	pthread_detach(server);

	// in main thread, we compress images and send them if there is a client.

	int flag = 1;
	int size;
	FILE *input;
	char buffer[SEND_BUFFER_SIZE] = {0};
	int readsize;
	while(1) {
		if(bufev && flag == 1) {
			struct evbuffer *output = bufferevent_get_output(bufev);

			if((input = fopen("shot-01.jpg", "rb")) == NULL) {
				DBG("open file failed!\n");
				return;
			}   
			    
			fseek(input, 0, SEEK_END);
			size = ftell(input);
			fseek(input, 0, SEEK_SET);
			sprintf(buffer, "HTTP/1.0 200 OK\r\n"\
				"Content-Type: image/jpeg\r\n"\
				"Content-Length: %d\r\n"\
				STD_HEADER \
				"\r\n", size);
			unsigned char* test = (unsigned char *) malloc(size + strlen(buffer));
			memcpy(test, buffer, strlen(buffer));
			readsize = fread(test + strlen(buffer), 1, size, input);
			if(readsize != size) {
				DBG("read image error!\n");
				return -1;
			}
//			evbuffer_add(output, buffer, strlen(buffer));
			evbuffer_add(output, test, size + strlen(buffer));
			writeready = 1;
			flag = 0;
			fclose(input);
			DBG("write image");
		}
	}

			
	i = 1;
	/*
	while(1) {
		// clean the formal jpeg data.
		memset(frame_buffer[i-1].data, 0, FRAME_BUFFER_SIZE);
		memset(name, 0, 20);
		sprintf(name, file, i);
		input_file = NULL;
		input_file = fopen(name, "rb");
		if(!input_file) {
			exit_global();
			err_sys("open file %s", name);
		}

		output_file = fopen("test.jpg", "wb");
		if(!output_file) {
			exit_global();
			err_sys("open file %s", "test.jpg");
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
		my_jpeg_stdio_dest(&cinfo, frame_buffer[i-1].data, &frame_buffer[i-1].length);

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

		fwrite(frame_buffer[i-1].data, sizeof(unsigned char), frame_buffer[i-1].length, output_file);
		fclose(output_file);

		fclose(input_file);
		free(imagedata);
		jpeg_destroy_compress(&cinfo);
		return 1;
	}
*/	
	exit_global();

}
