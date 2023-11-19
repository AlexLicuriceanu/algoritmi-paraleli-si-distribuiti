// Author: APD team, except where source was noted

#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#define CONTOUR_CONFIG_COUNT    16
#define FILENAME_MAX_SIZE       50
#define STEP                    8
#define SIGMA                   200
#define RESCALE_X               2048
#define RESCALE_Y               2048

#define CLAMP(v, min, max) if(v < min) { v = min; } else if(v > max) { v = max; }
#define MIN(a,b) (((a)<(b))?(a):(b))


// Updates a particular section of an image with the corresponding contour pixels.
// Used to create the complete contour image.
void update_image(ppm_image *image, ppm_image *contour, int x, int y) {
    for (int i = 0; i < contour->x; i++) {
        for (int j = 0; j < contour->y; j++) {
            int contour_pixel_index = contour->x * i + j;
            int image_pixel_index = (x + i) * image->y + y + j;

            image->data[image_pixel_index].red = contour->data[contour_pixel_index].red;
            image->data[image_pixel_index].green = contour->data[contour_pixel_index].green;
            image->data[image_pixel_index].blue = contour->data[contour_pixel_index].blue;
        }
    }
}


// Calls `free` method on the utilized resources.
void free_resources(ppm_image *image, ppm_image **contour_map, unsigned char **grid, int step_x) {
    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++) {
        free(contour_map[i]->data);
        free(contour_map[i]);
    }
    free(contour_map);

    for (int i = 0; i <= image->x / step_x; i++) {
        free(grid[i]);
    }
    free(grid);

    free(image->data);
    free(image);
}


// Arguments used inside the thread function.
struct thread_args {

    ppm_image* image;
    ppm_image* new_image;
    unsigned char** grid;
    ppm_image** contour_map;
    int step_x;
    int step_y;
    unsigned char sigma;

    int P;
    int thread_id;
    pthread_barrier_t* barrier;
};

void* marching_in_parallel(void* arg) {

    // Cast, unpack. 
    struct thread_args* args = (struct thread_args*) arg;

    ppm_image* image = args->image;
    ppm_image* new_image = args->new_image;
    unsigned char** grid = args->grid;
    ppm_image** contour_map = args->contour_map;
    int step_x = args->step_x;
    int step_y = args->step_y;
    unsigned char sigma = args->sigma;

    int P = args->P;
    int thread_id = args->thread_id;
    pthread_barrier_t* barrier = args->barrier;



    // Compute start and end bounds.
    int start = thread_id * (double) CONTOUR_CONFIG_COUNT / P;
    int end = MIN((thread_id + 1) * (double) CONTOUR_CONFIG_COUNT / P, CONTOUR_CONFIG_COUNT);



    // 0. Contour.
    // Creates a map between the binary configuration (e.g. 0110_2) and the corresponding pixels
    // that need to be set on the output image. An array is used for this map since the keys are
    // binary numbers in 0-15. Contour images are located in the './contours' directory.

    for (int i = start; i < end; i++) {
        char filename[FILENAME_MAX_SIZE];
        sprintf(filename, "./contours/%d.ppm", i);
        contour_map[i] = read_ppm(filename);
    }


    // 1. Rescaling.
    uint8_t sample[3];

    if (image->x > RESCALE_X && image->y > RESCALE_Y) {

        start = thread_id * (double) new_image->x / P;
        end = MIN((thread_id + 1) * (double) new_image->x / P, new_image->x);

        // use bicubic interpolation for scaling
        for (int i = start; i < end; i++) {
            for (int j = 0; j < new_image->y; j++) {
                float u = (float)i / (float)(new_image->x - 1);
                float v = (float)j / (float)(new_image->y - 1);
                sample_bicubic(image, u, v, sample);

                new_image->data[i * new_image->y + j].red = sample[0];
                new_image->data[i * new_image->y + j].green = sample[1];
                new_image->data[i * new_image->y + j].blue = sample[2];
            }
        }
        
        image = new_image;
    }

    // Wait for the rescaling to finish.
    pthread_barrier_wait(barrier);


    
    // 2. Sampling.
    // Corresponds to step 1 of the marching squares algorithm, which focuses on sampling the image.
    // Builds a p x q grid of points with values which can be either 0 or 1, depending on how the
    // pixel values compare to the `sigma` reference value. The points are taken at equal distances
    // in the original image, based on the `step_x` and `step_y` arguments.

    int p = image->x / step_x;
    int q = image->y / step_y;

    // Compute start and end bounds.
    start = thread_id * (double) p / P;
    end = MIN((thread_id + 1) * (double) p / P, p);

    for (int i = start; i < end; i++) {
        for (int j = 0; j < q; j++) {
            ppm_pixel curr_pixel = image->data[i * step_x * image->y + j * step_y];

            unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

            if (curr_color > sigma) {
                grid[i][j] = 0;
            } else {
                grid[i][j] = 1;
            }
        }
    }
    grid[p][q] = 0;


    // last sample points have no neighbors below / to the right, so we use pixels on the
    // last row / column of the input image for them    
    for (int i = start; i < end; i++) {
        ppm_pixel curr_pixel = image->data[i * step_x * image->y + image->x - 1];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > sigma) {
            grid[i][q] = 0;
        } else {
            grid[i][q] = 1;
        }
    }

    // Compute start and end bounds.
    start = thread_id * (double) q / P;
    end = MIN((thread_id + 1) * (double) q / P, q);

    for (int j = start; j < end; j++) {
        ppm_pixel curr_pixel = image->data[(image->x - 1) * image->y + j * step_y];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > sigma) {
            grid[p][j] = 0;
        } else {
            grid[p][j] = 1;
        }
    }

    // Wait for the sampling.
    pthread_barrier_wait(barrier);



    // 3. Marching.
    // Corresponds to step 2 of the marching squares algorithm, which focuses on identifying the
    // type of contour which corresponds to each subgrid. It determines the binary value of each
    // sample fragment of the original image and replaces the pixels in the original image with
    // the pixels of the corresponding contour image accordingly.

    // Compute start and end bounds.
    start = thread_id * (double) p / P;
    end = MIN((thread_id + 1) * (double) p / P, p);

   
    for (int i = start; i < end; i++) {
        for (int j = 0; j < q; j++) {
            unsigned char k = 8 * grid[i][j] + 4 * grid[i][j + 1] + 2 * grid[i + 1][j + 1] + 1 * grid[i + 1][j];
            update_image(image, contour_map[k], i * step_x, j * step_y);
        }
    }

    pthread_exit(NULL);
}



int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: ./tema1 <in_file> <out_file> <P>\n");
        return 1;
    }

    ppm_image *image = read_ppm(argv[1]);
    int step_x = STEP;
    int step_y = STEP;

    int original_x = image->x;
    int original_y = image->y;

    // Allocate memory for contour.
    ppm_image **map = (ppm_image **)malloc(CONTOUR_CONFIG_COUNT * sizeof(ppm_image *));
    if (!map) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    // Allocate memory for image.
    ppm_image *new_image = (ppm_image *)malloc(sizeof(ppm_image));
    if (!new_image) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }
    new_image->x = RESCALE_X;
    new_image->y = RESCALE_Y;

    new_image->data = (ppm_pixel*)malloc(new_image->x * new_image->y * sizeof(ppm_pixel));
    if (!new_image) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    // Allocate memory for grid.
    int p = MIN(image->x, new_image->x) / step_x;
    int q = MIN(image->y, new_image->x) / step_y;

    unsigned char **grid = (unsigned char **)malloc((p + 1) * sizeof(unsigned char*));
    if (!grid) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    for (int i = 0; i <= p; i++) {
        grid[i] = (unsigned char *)malloc((q + 1) * sizeof(unsigned char));
        if (!grid[i]) {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
    }


    

    int P = atoi(argv[3]);
    pthread_t threads[P];
    struct thread_args args[P];

    // Initialize the barrier.
    pthread_barrier_t barrier;
    if (pthread_barrier_init(&barrier, NULL, P) < 0) {
        fprintf(stderr, "Unable to init barrier\n");
        exit(1);
    }


    // Start the threads.
    for (int i = 0; i < P; i++) {
        
        args[i].image = image;
        args[i].new_image = new_image;
        args[i].grid = grid;
        args[i].contour_map = map;
        args[i].step_x = step_x;
        args[i].step_y = step_y;
        args[i].sigma = SIGMA;

        args[i].P = P;
        args[i].thread_id = i;
        args[i].barrier = &barrier;

        pthread_create(&threads[i], NULL, marching_in_parallel, &args[i]);
    }

    // Wait for the threads to finish.
    for (int i = 0; i < P; i++) {
        pthread_join(threads[i], NULL);
    }


    // Write output
    if (original_x > RESCALE_X && original_y > RESCALE_Y) {
        
        write_ppm(new_image, argv[2]);

        free_resources(new_image, map, grid, step_x);
        
    }
    else {

        write_ppm(image, argv[2]);

        free_resources(image, map, grid, step_x);
    }
    

    return 0;
}