#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#define size_yuv_420(w, h) (w*h + 2*((w+1)/2)*((h+1)/2))

/* ---- START - CXC Transformation Variables ---- */
double CXC_CF[4][4] = {{1, 1, 1, 1}, {2, 1,-1,-2}, {1,-1,-1, 1}, {1,-2, 2,-1}};
double CXC_CFT[4][4] = {{1, 2, 1, 1}, {1, 1,-1,-2}, {1,-1,-1, 2}, {1,-2, 1,-1}};
const double CXC_A = 0.5;
const double CXC_B = 0.6324; // sqrt((double)2/5);
const double CXC_EF[4][4] = {{0.25, 0.1581, 0.25, 0.1581}, {0.1581, 0.025, 0.1581, 0.025},
                             {0.25, 0.1581, 0.25, 0.1581}, {0.1581, 0.025, 0.1581, 0.025}};
double *CXC_CF_P[4][4];   // I do not like this solution.
double *CXC_CFT_P[4][4];  // I could research a better one in the future.
// double CXC_EF[4][4] = {{pow(CXC_A, 2), (CXC_A*CXC_B)/2, pow(CXC_A, 2), (CXC_A*CXC_B)/2}, {(CXC_A*CXC_B)/2, (pow(CXC_B, 2)/4), (CXC_A*CXC_B)/2, (pow(CXC_B, 2)/4)},
//                        {pow(CXC_A, 2), (CXC_A*CXC_B)/2, pow(CXC_A, 2), (CXC_A*CXC_B)/2}, {(CXC_A*CXC_B)/2, (pow(CXC_B, 2)/4), (CXC_A*CXC_B)/2, (pow(CXC_B, 2)/4)}};
/* ---- END - CXC Transformation Variables ---- */




 /* --------------------------------------------------------------------
    --------------------------- Declarations ---------------------------
    --------------------------------------------------------------------- */

void init_no_args(char *filename, uint16_t *width, uint16_t *height);
uint16_t intArg(char *arg);
void matrixMultiply(int N, double *mat1[][N], double *mat2[][N], double *res[][N]);
void YUVToDCT(int N, uint8_t *YUV[][N], int16_t *DCT[][N]);
void YUVToCXC(int N, uint8_t *YUV[][N], int16_t *CXC[][N]);
uint8_t* readYUVFrame(FILE *fp, uint16_t width, uint16_t height);
bool verifyYUVFile(FILE *fp, uint16_t width, uint16_t height);
uint32_t getFileSize(FILE *fp, uint64_t pointer_return);
uint32_t getTotalFrames(uint32_t total_size, uint16_t width, uint16_t height);

/**
 * uint8  -> max 256
 * uint16 -> max 65535
 * uint32 -> max 2,147,483,647  || (4K Resolution Covered)
 **/




 /* --------------------------------------------------------------------
    ----------------------------- Test Data ----------------------------
    --------------------------------------------------------------------- */

uint8_t  test_data[] = { 0,  1,  2,  3, /**/  4,  5,  6,  7, 
                         8,  9, 10, 11, /**/ 12, 13, 14, 15,
                        16, 17, 18, 19, /**/ 20, 21, 22, 23,
                        24, 25, 26, 27, /**/ 28, 29, 30, 31, 
                        //--------------------------------//
                        32, 33, 34, 35, /**/ 36, 37, 38, 39,
                        40, 41, 42, 43, /**/ 44, 45, 46, 47,
                        48, 49, 50, 51, /**/ 52, 53, 54, 55,
                        56, 57, 58, 59, /**/ 60, 61, 62, 63};

int main(int argc, char *argv[]){
    char *filename;
    uint16_t width, height;
    switch(argc){
        case 1:
            filename = malloc(256*sizeof(char));
            init_no_args(filename, &width, &height);
            break;
        case 4:
            filename = argv[1];
            width = intArg(argv[2]);
            height = intArg(argv[3]);
            break;
        default:
            puts("Invalid execution of program. Try:");
            printf("%s, or\n%s <path:name.yuv> <int:width> <int:height>\n", argv[0], argv[0]);
            return 1;
    }

    if(width == 0 || height == 0){
        puts("Incorrect arguments supplied. Width & height must be > 0.");
        return 1;
    }


    uint16_t sample_size = 4,
             total_samples = (width*height)/(sample_size*sample_size),
             columns = width/sample_size,
             rows = height/sample_size;

    uint8_t *YUV_data = test_data; // We fetch all YUV data and process Y only. An alternative would be to add
                                   // readY, read U, read V, skipY, skipU, skipV functions.
                                   // The caveat is that we allocate more memory than what we should to process
                                   // the Y frame.
    int16_t *DCT_Y_data = malloc(width*height*sizeof(int16_t)); // DCT can take negative values
    int16_t *CXC_Y_data = malloc(width*height*sizeof(int16_t)); // CXC can take negative values
    if(DCT_Y_data == NULL || CXC_Y_data == NULL){
        perror("Can't allocate enough memory for DCT_Y/CXC_Y table");
        return 1;
    }

    // Prepare constants used for CXC Transformation
    uint16_t i, j;
    for(i=0;i<4;i++){
        for(j=0;j<4;j++){ 
            CXC_CF_P[i][j] = &CXC_CF[i][j];
            CXC_CFT_P[i][j] = &CXC_CFT[i][j];
        }
    } 



 /* --------------------------------------------------------------------
    --------------------------- Read YUV file ---------------------------
    --------------------------------------------------------------------- */
    
    FILE *fp = fopen(filename, "rb");
	if(!fp){
		perror("Error opening YUV video for read");
		return false;
	}
    
    if(!verifyYUVFile(fp, width, height)) {
        fprintf(stderr, "The file supplied is probably not a YUV file of %dx%d quality.\n", width, height);
        return 1;
    }

    uint32_t file_size = getFileSize(fp, 0);
    uint32_t total_frames = getTotalFrames(file_size, width, height),
             frame;
    puts("Video Statistics:");
    printf(" - Quality: %dx%d\n", width, height);
    printf(" - Frames: %d\n", total_frames);
    printf(" - Sample size: %d\n - Samples per frame: %d (total %d)\n - Columns: %d\n - Rows: %d\n",
            sample_size, total_samples, total_samples*total_frames, columns, rows);



 /* --------------------------------------------------------------------
    --------------------- Init sub & energy tables ---------------------
    --------------------------------------------------------------------- */
    
    uint8_t *subtable_yuv[sample_size][sample_size]; // All subtables are tables of pointers
    int16_t *subtable_dct[sample_size][sample_size]; // so we can create an association between
    int16_t *subtable_cxc[sample_size][sample_size]; // 2D subtables and the 1D YUV/DCD/CXC arrays.
    double energy_yuv[sample_size][sample_size];
    double energy_dct[sample_size][sample_size];
    double energy_cxc[sample_size][sample_size];
    memset(energy_yuv, 0, sample_size*sample_size*sizeof(uint64_t));
    memset(energy_dct, 0, sample_size*sample_size*sizeof(uint64_t));
    memset(energy_cxc, 0, sample_size*sample_size*sizeof(uint64_t));



 /* ---------------------------------------------------------------------
    -------- YUV Read, DCT/CXC convertion & energy calculation ----------
    --------------------------------------------------------------------- */
    
    for(frame=0;frame<total_frames;frame++){
        YUV_data = readYUVFrame(fp, width, height);
        
        uint16_t sample_id, pos;
        for(sample_id=0;sample_id<total_samples;sample_id++){
            uint16_t curr_row = sample_id/columns;
            uint16_t curr_col = sample_id%columns;
            
            // Associate each sutable using pointers to the
            // actuall locations of each data.
            for(i=0;i<sample_size;i++){
                for(j=0;j<sample_size;j++){
                    pos = curr_row*sample_size*sample_size*columns + curr_col*sample_size + width*i + j;
                    
                    subtable_yuv[i][j] = &YUV_data[pos];  // Even though we are reading the whole YUV table, we only
                                                          // process the Y, since we take a specific number of
                                                          // total_samples.
                    subtable_dct[i][j] = &DCT_Y_data[pos]; 
                    subtable_cxc[i][j] = &CXC_Y_data[pos];
                }
            }

            YUVToDCT(sample_size, subtable_yuv, subtable_dct);
            YUVToCXC(sample_size, subtable_yuv, subtable_cxc);

            // Calculate the energy for each subtable
            for(i=0;i<sample_size;i++){
                for(j=0;j<sample_size;j++){
                    pos = curr_row*sample_size*sample_size*columns + curr_col*sample_size + width*i + j;
                    
                    energy_yuv[i][j] += (YUV_data[pos]*YUV_data[pos]);
                    energy_dct[i][j] += (DCT_Y_data[pos]*DCT_Y_data[pos]);
                    energy_cxc[i][j] += (CXC_Y_data[pos]*CXC_Y_data[pos]);
                }
            }
        }
    }

    double energy_sums[3] = {0, 0, 0};
    for(i=0;i<sample_size;i++){
        for(j=0;j<sample_size;j++){
            energy_sums[0] += energy_yuv[i][j] /= total_samples;
            energy_sums[1] += energy_dct[i][j] /= total_samples;
            energy_sums[2] += energy_cxc[i][j] /= total_samples;
        }
    }




 /* --------------------------------------------------------------------
    --------------------------- Print results --------------------------
    --------------------------------------------------------------------- */

    puts("------Output-------");
    puts("- Energy YUV:");
    for(i=0;i<sample_size;i++){
        printf("   ");
        for(j=0;j<sample_size;j++){
            printf("%12.0lf ", energy_yuv[i][j]);
        }
        puts("");
    }
    printf("  Sum: %.0lf\n", energy_sums[0]);

    puts("- Energy DCT:");
    for(i=0;i<sample_size;i++){
        printf("   ");
        for(j=0;j<sample_size;j++){
            printf("%12.0lf ", energy_dct[i][j]);
        }
        puts("");
    }
    printf("  Sum: %.0lf\n", energy_sums[1]);

    puts("- Energy CXC:");
    for(i=0;i<sample_size;i++){
        printf("   ");
        for(j=0;j<sample_size;j++){
            printf("%12.0lf ", energy_cxc[i][j]);
        }
        puts("");
    }
    printf("  Sum: %.0lf\n", energy_sums[2]);

    puts("Energies should be exactly the same.");
    puts("The difference in our scenarios are caused due to conversions to int, losing a lot of decimal points.");
    puts("These add up to create such energy differences, but we can see that they are approximately the same.");
    return 0;
}


void init_no_args(char *filename, uint16_t *width, uint16_t *height){
    int w, h;
    bool flag = false;
    if(filename == NULL){
        perror("File path malloc");
        exit(1);
    }
    puts(".......WELCOME........\n");
    puts("This project was created to study DCT transformations on YUV files as part of an academic assignment.");
    printf("\nPath: ");
    do{
        scanf("%255s", filename);
        if( access(filename, F_OK ) == -1 ) {
            puts("Path supplied is incorrect.");
            puts("There is no such file in that location.");
            printf("Path: ");
        }else{
            flag = true;
        }
    }while(!flag);
    
    printf("Width: ");
    do{
        scanf("%d", &w);
        if(w <= 0){
            puts("Width must be a number greater than 0.");
            printf("Width: ");
        }
    }while(w <= 0);

    printf("Height: ");
    do{
        scanf("%d", &h);
        if(w <= 0){
            puts("Height must be a number greater than 0.");
            printf("Height: ");
        }
    }while(h <= 0);

    *width = w;
    *height = h;
}


/*
 * Function:  intArg
 * --------------------
 *    Takes an argument supplied as char and checks  
 *  whether or not it is a valid integer.
 *  
 *  arg: The character that represents an integer.
 *  returns: The character as an integer value.
 */
uint16_t intArg(char *arg){
    uint16_t num;
    char *p;

    errno = 0;
    long conv = strtol(arg, &p, 10);

    // Check for errors: e.g., the string does not represent an integer
    // or the integer is larger than int
    if (errno != 0 || *p != '\0' || conv > INT_MAX || conv <= 0) {
        return 0;
    } else {
        num = (int) conv;
        return num;    
    }
}

/*
 * Function:  matrixMultiply
 * --------------------
 *    Multiplies a NxN matrix with another NxN matrix 
 * 
 *  N: The size of the NxN matrix.
 *  mat1: The first NxN matrix.
 *  mat2: The second NxN matrix.
 *  res: The output NxN matrix.
 *  returns: Fills 'res' matrix with the appropriate values after
 *    the multiplication of matrices mat1 & mat2.
 */
void matrixMultiply(int N, double *mat1[][N], double *mat2[][N], double *res[][N]) { 
    int i, j, k; 
    for (i = 0; i < N; i++) { 
        for (j = 0; j < N; j++) { 
            *res[i][j] = 0; 
            for (k = 0; k < N; k++) 
                *res[i][j] += *mat1[i][k] *  
                             *mat2[k][j];
        } 
    } 
} 


/*
 * Function:  YUVToDCT
 * --------------------
 *    Converts a YUV NxN matrix to a DCT NxN matrix.
 * 
 *  N: The size of the NxN matrix (for 4:2:0 usually 4).
 *  YUV: The YUV NxN matrix of associations.
 *  DCT: The DCT NxN matrix of associations.
 *  returns: Fills DCT matrix with the appropriate values after
 *    the DCT transformation from YUV.
 */
void YUVToDCT(int N, uint8_t *YUV[][N], int16_t *DCT[][N]){
    int i, j, x, y;
    double Cx, Cy;
    float sum=0;
    for (x=0;x<N;x++){
        for(y=0;y<N;y++){
            sum = 0;
            for(i=0;i<N;i++){
                for(j=0;j<N;j++){
                    sum += *YUV[i][j] * 
						cos((double)(2 * i + 1) * x * M_PI / (2 * N)) * 
						cos((double)(2 * j + 1) * y * M_PI / (2 * N)); 
                }
            }
            Cx = (x == 0 ? sqrt((double)1/N) : sqrt((double)2/N));
            Cy = (y == 0 ? sqrt((double)1/N) : sqrt((double)2/N));
            *DCT[x][y] = Cx*Cy*sum;
        }
    }
 }


 /*
 * Function:  YUVToCXC
 * --------------------
 *    Converts a YUV NxN matrix to a CXC NxN matrix.
 * 
 *  N: The size of the NxN matrix (for 4:2:0 usually 4).
 *  YUV: The YUV NxN matrix of associations.
 *  DCT: The DCT NxN matrix of associations.
 *  returns: Fills CXC matrix with the appropriate values after
 *    the CXC transformation from YUV.
 */
void YUVToCXC(int N, uint8_t *YUV[][N], int16_t *CXC[][N]){
    int i, j;
    double *tmp1_p[N][N], *tmp2_p[N][N];
    double tmp1[N][N], tmp2[N][N];
    for(i=0;i<N;i++){
        for(j=0;j<N;j++){
            tmp1[i][j] = *YUV[i][j];
            tmp1_p[i][j] = &tmp1[i][j];
            tmp2_p[i][j] = &tmp2[i][j];
        }
    }

    matrixMultiply(N, CXC_CF_P, tmp1_p, tmp2_p);
    matrixMultiply(N, tmp2_p, CXC_CFT_P, tmp1_p);

    
    for(i=0;i<N;i++){
        for(j=0;j<N;j++){
            *CXC[i][j] = *tmp1_p[i][j] * CXC_EF[i][j];
        }
    }
 }



/*
 * Function:  readYUVFrame
 * --------------------
 *    Reads the next YUV Frame from the fp.
 *  Tip: To go back to another frame, or skip some,
 *  you'll need to use 'fseek'.
 * 
 *  fp: Pointer pointing to the YUV Video file.
 *  width: The width of the YUV Video file.
 *  height: The height of the YUV Video file.
 * 
 *  returns: uint8t array containing the next YUV frame.
 */
uint8_t* readYUVFrame(FILE *fp, uint16_t width, uint16_t height){
    uint32_t frame_size = size_yuv_420(width, height);
    uint8_t *YUV = malloc(frame_size); /* sizeof not required since we are allocating uint8_t */
    if(YUV == NULL) return NULL;
	size_t result = fread(YUV, 1, frame_size, fp); /** READ 1 FRAME **/
	if (result != frame_size){
        free(YUV);
        return NULL;
    }
	return YUV;
}


/*
 * Function:  verifyYUVFile
 * --------------------
 *    Checks if the file that is being read is an appropriate yuv
 *  file by checking its file size.
 * 
 *  fp: Pointer pointing to the YUV Video file.
 *  width: The width of the YUV Video file.
 *  height: The height of the YUV Video file.
 * 
 *  returns: True if file is an acceptable YUV file, else false.
 */
bool verifyYUVFile(FILE *fp, uint16_t width, uint16_t height){
    uint32_t expected_size = size_yuv_420(width, height);
    uint32_t size = getFileSize(fp, 0);
    return size % expected_size == 0;
}


/*
 * Function:  getFileSize
 * --------------------
 *  fp: Pointer pointing to the YUV Video file.
 *  pointer_return: Location to return after file size calculation.
 * 
 *  returns: Size of the YUV Video file in bytes.
 */
uint32_t getFileSize(FILE *fp, uint64_t pointer_return){
    fseek(fp, 0, SEEK_END);
	uint32_t size = ftell(fp);
    fseek(fp, pointer_return, SEEK_SET);
    return size;
}


/*
 * Function:  getTotalFrames
 * --------------------
 *  total_size: File size returned from getFileSize(...) or a similar function. 
 *  width: The width of the YUV Video file.
 *  height: The height of the YUV Video file.
 * 
 *  returns: The total number of frames of the YUV Video file.
 *           returns 0 on error (total_size doesn't represent a YUV Video file of the same WxH)
 */
uint32_t getTotalFrames(uint32_t total_size, uint16_t width, uint16_t height){
    uint32_t expected_size = size_yuv_420(width, height);
    return total_size/expected_size;
}




// // read a raw yuv image file
// // raw yuv files can be generated by ffmpeg, for example, using :
// //  ffmpeg -i test.png -c:v rawvideo -pix_fmt yuv420p test.yuv
// // the returned image channels are contiguous, and Y stride=width, U and V stride=width/2
// // memory must be freed with free
// bool readRawYUV(const char *filename, uint32_t width, uint32_t height, uint8_t **YUV){
// 	FILE *fp = fopen(filename, "rb");
// 	if(!fp){
// 		perror("Error opening yuv image for read");
// 		return false;
// 	}
	
	
// 	uint32_t size = getFileSize(fp, 0);
//     uint32_t frames = 3;
//     // uint32_t expected_size = (width*height + 2*((width+1)/2)*((height+1)/2)) * frames; // 4:2:2
//     uint32_t expected_size = (width*height + 2*((width+1)/2)*((height+1)/2))*3; // 4:2:0
    
//     // uint32_t expected_size = width*height*(3/2)*300;
//     // size!=(  Y[w*h]     + 2* w*h/2  ) -- Explain
//     if(size!=expected_size){
// 		fprintf(stderr, "Wrong size of yuv image : %d bytes, expected %d bytes.\n", size, expected_size);
// 		fclose(fp);
// 		return false;
// 	}

//     *YUV = malloc(size);
// 	size_t result = fread(*YUV, 1, size, fp);
// 	if (result != size) {
// 		perror("Error reading yuv image");
// 		fclose(fp);
// 		return false;
// 	}
// 	fclose(fp);
// 	return true;
// }
