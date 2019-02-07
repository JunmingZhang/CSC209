#include <stdio.h>
#include <stdlib.h>
#include "bitmap.h"


/*
 * Read in the location of the pixel array, the image width, and the image 
 * height in the given bitmap file.
 */
void read_bitmap_metadata(FILE *image, int *pixel_array_offset, int *width, int *height) {
    int error;
    
    fseek(image, 10, SEEK_SET);
    error = fread(pixel_array_offset, sizeof(int), 1, image);
    if (error != 1) {
        fprintf(stderr, "invalid");
        exit(1);
    }
    fseek(image, 18, SEEK_SET);
    error = fread(width, sizeof(int), 1, image);
    if (error != 1) {
        fprintf(stderr, "invalid");
        exit(1);
    }
    fseek(image, 22, SEEK_SET);
    error = fread(height, sizeof(int), 1, image);
    if (error != 1) {
        fprintf(stderr, "invalid");
        exit(1);
    }
    rewind(image);
}

/*
 * Read in pixel array by following these instructions:
 *
 * 1. First, allocate space for m `struct pixel *` values, where m is the 
 *    height of the image.  Each pointer will eventually point to one row of 
 *    pixel data.
 * 2. For each pointer you just allocated, initialize it to point to 
 *    heap-allocated space for an entire row of pixel data.
 * 3. Use the given file and pixel_array_offset to initialize the actual 
 *    struct pixel values. Assume that `sizeof(struct pixel) == 3`, which is 
 *    consistent with the bitmap file format.
 *    NOTE: We've tested this assumption on the Teaching Lab machines, but 
 *    if you're trying to work on your own computer, we strongly recommend 
 *    checking this assumption!
 * 4. Return the address of the first `struct pixel *` you initialized.
 */
struct pixel **read_pixel_array(FILE *image, int pixel_array_offset, int width, int height) {
    struct pixel ** struct_struct_ptr = malloc(sizeof(struct pixel *) * height);
    if (struct_struct_ptr == NULL) {
        fprintf(stderr, "invalid");
        exit(1); 
    }
    
    fseek(image, pixel_array_offset, SEEK_SET);
    
    for (int count = 0; count < height; count++) {
        struct_struct_ptr[count] = malloc(sizeof(struct pixel) * width);
        if (struct_struct_ptr[count] == NULL) {
            fprintf(stderr, "invalid");
            exit(1);
        }
        for (int row_count = 0; row_count < width; row_count++) {
            int error;
            struct pixel pixel_struct;
            error = fread(&(pixel_struct.blue), sizeof(unsigned char), 1, image);
            if (error != 1) {
                fprintf(stderr, "invalid");
                exit(1);
            }
            
            error = fread(&(pixel_struct.green), sizeof(unsigned char), 1, image);
            if (error != 1) {
                fprintf(stderr, "invalid");
                exit(1);
            }
            
            error = fread(&(pixel_struct.red), sizeof(unsigned char), 1, image);
            if (error != 1) {
                fprintf(stderr, "invalid");
                exit(1);
            }
            
            struct_struct_ptr[count][row_count] = pixel_struct;
        }
    }
    return struct_struct_ptr;
}


/*
 * Print the blue, green, and red colour values of a pixel.
 * You don't need to change this function.
 */
void print_pixel(struct pixel p) {
    printf("(%u, %u, %u)\n", p.blue, p.green, p.red);
}
