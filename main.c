#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>


void        init_no_args(uint8_t **YUV_data);
uint32_t    getFileSize(FILE *fp, uint32_t pointer_return);
bool        readRawYUV(const char *filename, uint32_t width, uint32_t height, uint8_t **YUV);

int main(int argc, char *argv[]){
    uint8_t *YUV_data;
    switch(argc){
        case 1:
            init_no_args(&YUV_data);
            break;
        case 5:
            readRawYUV(argv[1], 176, 144, &YUV_data);  
            break;
        default:
            puts("Invalid execution of program. Try: \n");
            printf("%s, or\n %s <path:name.yuv> <int:width> <int:height> <int:frame_rate>\n", argv[0], argv[0]);
            break;
    }
    return 0;
}


void init_no_args(uint8_t **YUV_data){
    char path[256] = "";
    uint8_t width, height, fps;
    puts(".......WELCOME........\n");
    puts("Path: ");
    do{
        scanf("%256s", path);
    }while(path == '\0');
    
    printf("Project: %s\n", path);
    readRawYUV(path, 176, 144, YUV_data);  
}

// read a raw yuv image file
// raw yuv files can be generated by ffmpeg, for example, using :
//  ffmpeg -i test.png -c:v rawvideo -pix_fmt yuv420p test.yuv
// the returned image channels are contiguous, and Y stride=width, U and V stride=width/2
// memory must be freed with free
bool readRawYUV(const char *filename, uint32_t width, uint32_t height, uint8_t **YUV){
	FILE *fp = fopen(filename, "rb");
	if(!fp){
		perror("Error opening yuv image for read");
		return false;
	}
	
	
	uint32_t size = getFileSize(fp, 0);
    uint32_t frames = 8;
    uint32_t expected_size = (width*height + 2*((width+1)/2)*((height+1)/2)) * frames;
    
    // uint32_t expected_size = width*height*(3/2)*300;
    // size!=(  Y[w*h]     + 2* w*h/2  ) -- Explain
    if(size!=expected_size){
		fprintf(stderr, "Wrong size of yuv image : %d bytes, expected %d bytes.\n", size, expected_size);
		fclose(fp);
		return false;
	}

    *YUV = malloc(size);
	size_t result = fread(*YUV, 1, size, fp);
	if (result != size) {
		perror("Error reading yuv image");
		fclose(fp);
		return false;
	}
	fclose(fp);
	return true;
}

/*
    Input:
        fp -> Pointer pointing to a file.
        pointer_return -> Location to return after file size calculation.
    Returns: 
        uint32_t -> Size of a file in bytes.
*/
uint32_t getFileSize(FILE *fp, uint32_t pointer_return){
    fseek(fp, 0, SEEK_END);
	uint32_t size = ftell(fp);
    fseek(fp, pointer_return, SEEK_SET);
    return size;
}
