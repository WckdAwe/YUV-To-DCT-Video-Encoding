#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>
#include <string.h>


double to_rad(double);
void multiply(int N,
              double mat1[][N],  
              double mat2[][N],  
              double res[][N]);
void yuv_to_dct(int N, int M, int16_t* DCTMatrix[][M], uint8_t *Matrix[][M]);
void        init_no_args(uint8_t **YUV_data);
uint32_t    getFileSize(FILE *fp, uint32_t pointer_return);
bool        readRawYUV(const char *filename, uint32_t width, uint32_t height, uint8_t **YUV);

int main(int argc, char *argv[]){
        // uint32_t width=176, height=144;

    // switch(argc){
    //     case 1:
    //         init_no_args(&YUV_data);
    //         break;
    //     case 2:
    //         readRawYUV(argv[1], 176, 144, &YUV_data);  
    //         break;
    //     default:
    //         puts("Invalid execution of program. Try: \n");
    //         printf("%s, or\n %s <path:name.yuv> <int:width> <int:height> <int:frame_rate>\n", argv[0], argv[0]);
    //         break;
    // }

    /**
     * uint8  -> max 256
     * uint16 -> max 65535
     * uint32 -> max 2,147,483,647  || (4K Resolution Covered)
     **/
    /**
     * YUV Reading
     */ 

    uint8_t  test_data[] = { 0,  1,  2,  3, /**/  4,  5,  6,  7, 
                             8,  9, 10, 11, /**/ 12, 13, 14, 15,
                            16, 17, 18, 19, /**/ 20, 21, 22, 23,
                            24, 25, 26, 27, /**/ 28, 29, 30, 31, 
                            //--------------------------------//
                            32, 33, 34, 35, /**/ 36, 37, 38, 39,
                            40, 41, 42, 43, /**/ 44, 45, 46, 47,
                            48, 49, 50, 51, /**/ 52, 53, 54, 55,
                            56, 57, 58, 59, /**/ 60, 61, 62, 63};
    // uint8_t  test_data[] = { 255,  255,  255,  255, /**/  4,  5,  6,  7, 
    //                          255,  255,  255,  255, /**/ 12, 13, 14, 15,
    //                          255,  255,  255,  255, /**/ 20, 21, 22, 23,
    //                          255,  255,  255,  255, /**/ 28, 29, 30, 31, 
    //                         //--------------------------------//
    //                         32, 33, 34, 35, /**/ 36, 37, 38, 39,
    //                         40, 41, 42, 43, /**/ 44, 45, 46, 47,
    //                         48, 49, 50, 51, /**/ 52, 53, 54, 55,
    //                         56, 57, 58, 59, /**/ 60, 61, 62, 63};
    uint16_t width=8, height=8; 
    uint32_t frame_size = width*height + 2*((width+1)/2)*((height+1)/2);
    int frames = 3;

    uint16_t sample_size = 4,
             total_samples = (width*height)/(sample_size*sample_size),
             columns = width/sample_size,
             rows = height/sample_size;

    uint8_t *YUV_Y_data = test_data;
    int16_t *DCT_Y_data = malloc(width*height*sizeof(int16_t)); // DCT can take negative values
    if(DCT_Y_data == NULL){
        perror("Can't allocate enough memory for DCT_Y table");
        return 1;
    }

    puts("Video Statistics:");
    printf(" - Quality: %dx%d\n", width, height);
    printf(" - Sample size: %d\n - Samples per frame: %d (total %d)\n - Columns: %d\n - Rows: %d\n",
            sample_size, total_samples, total_samples*frames, columns, rows);

    // Print Samples
    uint8_t *subtable_yuv[sample_size][sample_size];
    int16_t *subtable_dct[sample_size][sample_size];
    uint64_t energy_pixel[sample_size][sample_size];
    uint64_t energy_coef[sample_size][sample_size];
    memset(energy_pixel, 0, sample_size*sample_size*sizeof(uint64_t));
    memset(energy_coef, 0, sample_size*sample_size*sizeof(uint64_t));
    uint16_t sample_id, i, j, pos;
    for(sample_id=0;sample_id<total_samples;sample_id++){
        printf("=====[%d]=====\n", sample_id);
        uint16_t curr_row = sample_id/columns;
        uint16_t curr_col = sample_id%columns;
        
        // Create YUV table
        for(i=0;i<sample_size;i++){
            for(j=0;j<sample_size;j++){
                pos = curr_row*sample_size*sample_size*columns + curr_col*sample_size + width*i + j;
                subtable_yuv[i][j] = &YUV_Y_data[pos];
                subtable_dct[i][j] = &DCT_Y_data[pos];
                printf("%d ", YUV_Y_data[pos]);
            }
            puts("");
        }
        yuv_to_dct(sample_size, sample_size, subtable_dct, subtable_yuv);
        puts("-----DCT Conversion-----");
        // Print DCT table
        for(i=0;i<sample_size;i++){
            for(j=0;j<sample_size;j++){
                pos = curr_row*sample_size*sample_size*columns + curr_col*sample_size + width*i + j;
                
                energy_pixel[i][j] += (YUV_Y_data[pos]*YUV_Y_data[pos]);
                energy_coef[i][j] += (DCT_Y_data[pos]*DCT_Y_data[pos]);
            }
            puts("");
        }
        puts("=============");

    }

    // Print energy Coef
    puts("----- Energy Pixel -----");
    for(i=0;i<sample_size;i++){
        for(j=0;j<sample_size;j++){
            energy_pixel[i][j] /= total_samples;
            printf("%d", energy_pixel[i][j]);
        }
        puts("");
    }

    puts("----- Energy Coef -----");
    for(i=0;i<sample_size;i++){
        for(j=0;j<sample_size;j++){
            energy_coef[i][j] /= total_samples;
            printf("%d ", energy_coef[i][j]);
        }
        puts("");
    }

    puts("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=");
    double res[4][4], res2[4][4];
    double a = 0.5, b = sqrt((double)2/5);
    double Cf[4][4] = { 
        {1, 1, 1, 1},
        {2, 1,-1,-2},
        {1,-1,-1, 1},
        {1,-2, 2,-1},
    };
    double CfT[4][4] = { 
        {1, 2, 1, 1},
        {1, 1,-1,-2},
        {1,-1,-1, 2},
        {1,-2, 1,-1},
    };
    double exampleTable[4][4] = {
      { 5, 8,11,10},
      { 9, 8, 4,12},
      { 1,10,11, 4},
      {19, 6,15, 7}  
    };
    double Ef[4][4] = {
        {pow(a, 2), (a*b)/2, pow(a, 2), (a*b)/2},
        {(a*b)/2, (pow(b, 2)/4), (a*b)/2, (pow(b, 2)/4)},
        {pow(a, 2), (a*b)/2, pow(a, 2), (a*b)/2},
        {(a*b)/2, (pow(b, 2)/4), (a*b)/2, (pow(b, 2)/4)},
    };
    multiply(4, Cf, exampleTable, res);
    multiply(4, res, CfT, res2);

    for(i=0;i<4;i++){
        for(j=0;j<4;j++){
            res2[i][j] *= Ef[i][j];
            // printf("%lf %lf %lf %lf %lf\n", Cf[i][j], CfT[i][j],
            //                         exampleTable[i][j], Ef[i][j],
            //                         res[i][j]);
            printf("%.3lf ", res2[i][j]);
            // sleep(5);
        }
        puts("");
    }
    return 0;
}

void multiply(int N,
              double mat1[][N],  
              double mat2[][N],  
              double res[][N]) { 
    int i, j, k; 
    for (i = 0; i < N; i++) 
    { 
        for (j = 0; j < N; j++) 
        { 
            res[i][j] = 0; 
            for (k = 0; k < N; k++) 
                res[i][j] += mat1[i][k] *  
                             mat2[k][j]; 
        } 
    } 
} 

void yuv_to_dct(int N, int M, int16_t* DCTMatrix[][M], uint8_t *Matrix[][M]){
    int i, j, x, y;
    double Cx, Cy;
    float sum=0;
    for (x=0;x<N;x++){
        for(y=0;y<M;y++){
            sum = 0;
            for(i=0;i<N;i++){
                for(j=0;j<M;j++){
                    sum += *Matrix[i][j] * 
						cos((double)(2 * i + 1) * x * 3.14 / (2 * N)) * 
						cos((double)(2 * j + 1) * y * 3.14 / (2 * M)); 
                }
            }
            Cx = (x == 0 ? sqrt((double)1/N) : sqrt((double)2/N));
            Cy = (y == 0 ? sqrt((double)1/N) : sqrt((double)2/N));
            *DCTMatrix[x][y] = Cx*Cy*sum;
        }
    }
 }



//  

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
    uint32_t frames = 3;
    // uint32_t expected_size = (width*height + 2*((width+1)/2)*((height+1)/2)) * frames; // 4:2:2
    uint32_t expected_size = (width*height + 2*((width+1)/2)*((height+1)/2))*3; // 4:2:0
    
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
