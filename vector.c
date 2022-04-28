#include <math.h>
#include <string.h>
#include <stdlib.h>

#include <omp.h>

#include "ppm.h"

// Image from:
// http://7-themes.com/6971875-funny-flowers-pictures.html

typedef struct {
    float red,green,blue;
} AccuratePixel;

typedef float v4sf __attribute__ ((vector_size(16))); // vector of four single floats

union f4vector 
{
  v4sf v;
  float f[4];
};

typedef struct {
	int x, y;
	union f4vector *data;
} FloatImage;

typedef struct {
	int x, y;
	AccuratePixel *data;
} AccurateImage;

// Convert ppm to high precision format.
FloatImage *convertToAccurateImage(PPMImage *image) {
	// Make a copy
	FloatImage *imageAccurate;
	imageAccurate = (FloatImage *)malloc(sizeof(FloatImage));
	imageAccurate->data = (union f4vector*)malloc(image->x * image->y * sizeof(v4sf));
	#pragma omp parallel for
	for(int i = 0; i < image->x * image->y; i++) {
		imageAccurate->data[i].f[0] = (float) image->data[i].red;
		imageAccurate->data[i].f[1] = (float) image->data[i].green;
		imageAccurate->data[i].f[2] = (float) image->data[i].blue;
		imageAccurate->data[i].f[3] = 0.f;
	}
	imageAccurate->x = image->x;
	imageAccurate->y = image->y;
	
	return imageAccurate;
}

AccurateImage *convertToAccurateImageReal(FloatImage *image) {
	// Make a copy
	AccurateImage *imageAccurate;
	imageAccurate = (AccurateImage *)malloc(sizeof(AccurateImage));
	imageAccurate->data = (AccuratePixel*)malloc(image->x * image->y * sizeof(AccuratePixel));
	for(int i = 0; i < image->x * image->y; i++) {
		imageAccurate->data[i].red   = image->data[i].f[0];
		imageAccurate->data[i].green = image->data[i].f[1];
		imageAccurate->data[i].blue  = image->data[i].f[2];
	}
	imageAccurate->x = image->x;
	imageAccurate->y = image->y;
	
	return imageAccurate;
}

PPMImage * convertToPPPMImage(AccurateImage *imageIn) {
    PPMImage *imageOut;
    imageOut = (PPMImage *)malloc(sizeof(PPMImage));
    imageOut->data = (PPMPixel*)malloc(imageIn->x * imageIn->y * sizeof(PPMPixel));

    imageOut->x = imageIn->x;
    imageOut->y = imageIn->y;
	#pragma omp parallel for
    for(int i = 0; i < imageIn->x * imageIn->y; i++) {
        imageOut->data[i].red = imageIn->data[i].red;
        imageOut->data[i].green = imageIn->data[i].green;
        imageOut->data[i].blue = imageIn->data[i].blue;
    }
    return imageOut;
}

// blur one color channel
void blurIteration(FloatImage *imageOut, FloatImage *imageIn, int size) {
	// TODO optimizations to be made here!
	// Iterate over each pixel
	int numberOfValuesInEachRow = imageIn->x;

	#pragma omp parallel for
	for(int senterY = 0; senterY < imageIn->y; senterY++) {
		// TODO do they need to be inside here?
		union f4vector sum;
		sum.f[0] = 0;
		sum.f[1] = 0;
		sum.f[2] = 0;
		sum.f[3] = 0;
		int countIncluded = 0;
		int senterYoffset = numberOfValuesInEachRow * senterY;
		for(int senterX = 0; senterX < imageIn->x; senterX++) {
			// TODO this can be improved (will take some work though)
			// TODO can save the value above for even faster computations(?)
			
			if(senterX != 0){
				for(int i = -size; i <= size; i++) {
					// Remove x values
					if((senterY + i) < 0 || senterY + i >= imageIn->y) continue;	
					if((senterX - size - 1) >= 0){
						int offsetOfThePixelRemove = (numberOfValuesInEachRow * (senterY + i) + (senterX - size - 1));
						sum.v -= imageIn->data[offsetOfThePixelRemove].v;
						countIncluded--;
					}
					if((senterX + size) < imageIn->x){
						int offsetOfThePixelAdd = (numberOfValuesInEachRow * (senterY + i) + (senterX + size));
						sum.v += imageIn->data[offsetOfThePixelAdd].v;
						countIncluded++;
					}
				}
			}
			else {
				sum.f[0] = 0;
				sum.f[1] = 0;
				sum.f[2] = 0;
				sum.f[3] = 0;
				countIncluded = 0;
				for(int y = -size; y <= size; y++) {
					int currentY = senterY + y;
					// Check if we are outside the bounds
					if(currentY < 0)
						continue;
					if(currentY >= imageIn->y)
						continue;
							
					int currentYoffset = numberOfValuesInEachRow * currentY;
					for(int x = -size; x <= size; x++) {

						// Check if we are outside the bounds
						int currentX = senterX + x;
						if(currentX < 0)
							continue;
						if(currentX >= imageIn->x)
							continue;

						// Now we can begin
						
						int offsetOfThePixel = (currentYoffset + currentX);

						sum.v += imageIn->data[offsetOfThePixel].v;

						// Keep track of how many values we have included
						countIncluded++;
					}
				}
			}
			
			// Now we compute the final value
			union f4vector value;
            // TODO could be wrong
			float countIncludedFloat = (float) countIncluded;
			
            
			value.v = sum.v / countIncludedFloat;

			// Update the output image
			int offsetOfThePixel = (senterYoffset + senterX);
			imageOut->data[offsetOfThePixel].v = value.v;
            
			
		}

	}
	
}


// Perform the final step, and return it as ppm.
PPMImage * imageDifference(AccurateImage *imageInSmall, AccurateImage *imageInLarge) {
	PPMImage *imageOut;
	imageOut = (PPMImage *)malloc(sizeof(PPMImage));
	imageOut->data = (PPMPixel*)malloc(imageInSmall->x * imageInSmall->y * sizeof(PPMPixel));
	
	imageOut->x = imageInSmall->x;
	imageOut->y = imageInSmall->y;

	for(int i = 0; i < imageInSmall->x * imageInSmall->y; i++) {
		float value = (imageInLarge->data[i].red - imageInSmall->data[i].red);
		if(value > 255)
			imageOut->data[i].red = 255;
		else if (value < -1.0) {
			value = 257.0+value;
			if(value > 255)
				imageOut->data[i].red = 255;
			else
				imageOut->data[i].red = floor(value);
		} else if (value > -1.0 && value < 0.0) {
			imageOut->data[i].red = 0;
		} else {
			imageOut->data[i].red = floor(value);
		}

		value = (imageInLarge->data[i].green - imageInSmall->data[i].green);
		if(value > 255)
			imageOut->data[i].green = 255;
		else if (value < -1.0) {
			value = 257.0+value;
			if(value > 255)
				imageOut->data[i].green = 255;
			else
				imageOut->data[i].green = floor(value);
		} else if (value > -1.0 && value < 0.0) {
			imageOut->data[i].green = 0;
		} else {
			imageOut->data[i].green = floor(value);
		}

		value = (imageInLarge->data[i].blue - imageInSmall->data[i].blue);
		if(value > 255)
			imageOut->data[i].blue = 255;
		else if (value < -1.0) {
			value = 257.0+value;
			if(value > 255)
				imageOut->data[i].blue = 255;
			else
				imageOut->data[i].blue = floor(value);
		} else if (value > -1.0 && value < 0.0) {
			imageOut->data[i].blue = 0;
		} else {
			imageOut->data[i].blue = floor(value);
		}
	}
	return imageOut;
}


int main(int argc, char** argv) {
    // read image
    PPMImage *image;
    // select where to read the image from
    if(argc > 1) {
        // from file for debugging (with argument)
        image = readPPM("flower.ppm");
    } else {
        // from stdin for cmb
        image = readStreamPPM(stdin);
    }
	
	
	FloatImage *imageAccurate1_tiny = convertToAccurateImage(image);
	FloatImage *imageAccurate2_tiny = convertToAccurateImage(image);
	
	// Process the tiny case:
	
	int size = 2;
	blurIteration(imageAccurate2_tiny, imageAccurate1_tiny, size);
	blurIteration(imageAccurate1_tiny, imageAccurate2_tiny, size);
	blurIteration(imageAccurate2_tiny, imageAccurate1_tiny, size);
	blurIteration(imageAccurate1_tiny, imageAccurate2_tiny, size);
	blurIteration(imageAccurate2_tiny, imageAccurate1_tiny, size);
	
	
	
	FloatImage *imageAccurate1_small = convertToAccurateImage(image);
	FloatImage *imageAccurate2_small = convertToAccurateImage(image);
	
	// Process the small case:
	
	size = 3;
	blurIteration(imageAccurate2_small, imageAccurate1_small, size);
	blurIteration(imageAccurate1_small, imageAccurate2_small, size);
	blurIteration(imageAccurate2_small, imageAccurate1_small, size);
	blurIteration(imageAccurate1_small, imageAccurate2_small, size);
	blurIteration(imageAccurate2_small, imageAccurate1_small, size);
	

    // an intermediate step can be saved for debugging like this
//    writePPM("imageAccurate2_tiny.ppm", convertToPPPMImage(imageAccurate2_tiny));
	
	FloatImage *imageAccurate1_medium = convertToAccurateImage(image);
	FloatImage *imageAccurate2_medium = convertToAccurateImage(image);
	
	// Process the medium case:
	size = 5;
	blurIteration(imageAccurate2_medium, imageAccurate1_medium, size);
	blurIteration(imageAccurate1_medium, imageAccurate2_medium, size);
	blurIteration(imageAccurate2_medium, imageAccurate1_medium, size);
	blurIteration(imageAccurate1_medium, imageAccurate2_medium, size);
	blurIteration(imageAccurate2_medium, imageAccurate1_medium, size);

	
	FloatImage *imageAccurate1_large = convertToAccurateImage(image);
	FloatImage *imageAccurate2_large = convertToAccurateImage(image);
	
	// Do each color channel
	size = 8;
	blurIteration(imageAccurate2_large, imageAccurate1_large, size);
	blurIteration(imageAccurate1_large, imageAccurate2_large, size);
	blurIteration(imageAccurate2_large, imageAccurate1_large, size);
	blurIteration(imageAccurate1_large, imageAccurate2_large, size);
	blurIteration(imageAccurate2_large, imageAccurate1_large, size);
	
   	AccurateImage *final_tiny2 = convertToAccurateImageReal(imageAccurate2_tiny);
	AccurateImage *final_small2 = convertToAccurateImageReal(imageAccurate2_small);
	AccurateImage *final_medium2 = convertToAccurateImageReal(imageAccurate2_medium);
	AccurateImage *final_large2 =convertToAccurateImageReal(imageAccurate2_large);
	
	
	// calculate difference
	// calculate difference
	PPMImage *final_tiny = imageDifference(final_tiny2, final_small2);
    PPMImage *final_small = imageDifference(final_small2, final_medium2);
    PPMImage *final_medium = imageDifference(final_medium2, final_large2);
	// Save the images.
    if(argc > 1) {
        writePPM("flower_tiny.ppm", final_tiny);
        writePPM("flower_small.ppm", final_small);
        writePPM("flower_medium.ppm", final_medium);
    } else {
        writeStreamPPM(stdout, final_tiny);
        writeStreamPPM(stdout, final_small);
        writeStreamPPM(stdout, final_medium);
    }
}

