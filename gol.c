/*
 * Swarthmore College, CS 31
 * Copyright (c) 2023 Swarthmore College Computer Science Department,
 * Swarthmore PA
 */

/*This file implements a program to play Conway's Game of Life. The program
reads in an input file that specifies the grid size and number of iterations
of the game to play, then lists a number of coordinates that start off
as alive. The game can be played in three output modes: no output, an
ascii animation output, and a ParaVisi animation output mode. In the rounds
of the game, a live cell dies if there are 0 or 1 live neighbors. If there
are 2 or 3 live neighbors, the cell lives, and if the live cell has 4 neighbors
it will die. A dead cell with 3 live neighbors will come alive and will
stay dead with any other number of live neighbors. Each cell has eight
neighbors. This includes the corners and edges, they wrap around to the
opposite side of the board. At the end of the ascii and no output runs,
the total runtime of the program is printed, along with the number of live
cells at the end of the final round.

Additionally, the user can specify the number of threads they want the program
to run on. The user can also choose whether the threads are partitioned
in the board by row or column.

The code was successfully tested with the files: test_corners.txt, 
test_corners2.txt, test_edges.txt, and test_bigger.txt. 
*/

/*
before submitting
valgrind?
comments?
paravisi quit issue
*/

/*
 * To run:
 * ./gol file1.txt  0  # run with config file file1.txt, do not print board
 * ./gol file1.txt  1  # run with config file file1.txt, ascii animation
 * ./gol file1.txt  2  # run with config file file1.txt, ParaVis animation
 *
 */
#include <pthreadGridVisi.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "colors.h"

/****************** Definitions **********************/
/* Three possible modes in which the GOL simulation can run */
#define OUTPUT_NONE   (0)   // with no animation
#define OUTPUT_ASCII  (1)   // with ascii animation
#define OUTPUT_VISI   (2)   // with ParaVis animation

/* Used to slow down animation run modes: usleep(SLEEP_USECS);
 * Change this value to make the animation run faster or slower
 */
#define SLEEP_USECS    (100000)

/* A global variable to keep track of the number of live cells in the
 * world (this is the ONLY global variable you may use in your program)
 */

/* This struct represents all the data you need to keep track of your GOL
 * simulation.  Rather than passing individual arguments into each function,
 * we'll pass in everything in just one of these structs.
 * this is passed to play_gol, the main gol playing loop
 *
 * NOTE: You will need to use the provided fields here, but you'll also
 *       need to add additional fields. (note the nice field comments!)
 * NOTE: DO NOT CHANGE THE NAME OF THIS STRUCT!!!!
 */
struct gol_data {

    // NOTE: DO NOT CHANGE the names of these 4 fields (but USE them)
    int rows;  // the row dimension
    int cols;  // the column dimension
    int iters; // number of iterations to run the gol simulation
    int output_mode; // set to:  OUTPUT_NONE, OUTPUT_ASCII, or OUTPUT_VISI

    // TODO: add more fields for gol_data that your play_gol function needs
    int* world;
    int* world_copy;
    int divide_mode; // 1 is col, 0 is row
    int threads;
    int print;

    int id;
    int row_start;
    int row_end;
    int col_start;
    int col_end;
    int mini_rows;  // the row dimension
    int mini_cols;


    /* fields used by ParaVis library (when run in OUTPUT_VISI mode). */
    // NOTE: DO NOT CHANGE their definitions BUT USE these fields
    visi_handle handle;
    color3 *image_buff;
};

/****************** Function Prototypes **********************/
/* the main gol game playing loop (prototype must match this) */
void *play_gol(void *args);
void *print_stats(void *args);
/* init gol data from the input file and run mode cmdline args */
int init_game_data_from_args(struct gol_data *data, char **argv);
/* print board to the terminal (for OUTPUT_ASCII mode) */
void print_board(struct gol_data *data, int round);

int openfile(struct gol_data *data, FILE *infile);
void make_world(struct gol_data *data, char **argv, FILE *infile);
void update_cells(struct gol_data *data);
int check_neighbors(struct gol_data *data, int row, int col);
void update_colors(struct gol_data *data);
void validation(int argc, char **argv, struct gol_data* data);
void partition(struct gol_data *data, pthread_t *tid, struct gol_data* targs);
/**************************************************************/


/************ Definitions for using ParVisi library ***********/
/* initialization for the ParaVisi library (DO NOT MODIFY) */
int setup_animation(struct gol_data* data);
/* name for visi (you may change the string value if you'd like) */
static char visi_name[] = "GOL!";
/**************************************************************/

static int total_live = 0;
static pthread_barrier_t done;
static pthread_mutex_t my_mutex;

/************************ Main Function ***********************/
int main(int argc, char **argv) {

    int ret, i;
    struct gol_data data;
    double secs;
    struct timeval start_time, stop_time;

    pthread_t *tid;
    struct gol_data *targs;  // Arg passed into each thread


    if (pthread_mutex_init(&my_mutex, NULL)) { 
        printf("pthread_mutex_init error\n");
        exit(1);
    }

    /* check command line arguments */
    validation(argc, argv, &data);

    // Allocate tid
    tid = malloc(sizeof(pthread_t) * data.threads);
    if (!tid) { perror("malloc: pthread_t array"); exit(1); }

    targs = malloc(sizeof(struct gol_data) * data.threads);
    if (!targs) { perror("malloc: targs array"); exit(1); }

    ret = pthread_barrier_init(&done, NULL, data.threads);
    if (ret != 0) {perror("pthread_barrier_init"); exit(1); }

    /* initialize ParaVisi animation (if applicable) */
    if (data.output_mode == OUTPUT_VISI) {
        setup_animation(&data);
        
    }
    
    partition(&data, tid, targs);

    /* ASCII output: clear screen & print the initial board */
    if (data.output_mode == OUTPUT_ASCII) {
        if (system("clear")) { perror("clear"); exit(1); }
        print_board(&data, 0);
    }
    
    /* Invoke play_gol in different ways based on the run mode */


    if (data.output_mode == OUTPUT_NONE) {  // run with no animation
        ret = gettimeofday(&start_time, NULL);
        for (i = 0; i < data.threads; i++) {
            ret = pthread_create(&tid[i], 0, play_gol, &targs[i]);
        }
        if (ret) { perror("Error pthread_create\n"); exit(1); }
    }

    else if (data.output_mode == OUTPUT_ASCII) { // run with ascii animation
        ret = gettimeofday(&start_time, NULL);
        
        for (i = 0; i < data.threads; i++) {
            ret = pthread_create(&tid[i], 0, play_gol, &targs[i]);
        }
        if (ret) { perror("Error pthread_create\n"); exit(1); }

        // clear the previous print_board output from the terminal:
        if (system("clear")) { perror("clear"); exit(1); }

    }
    else if (data.output_mode == OUTPUT_VISI) {  
        // OUTPUT_VISI: run with ParaVisi animation
        // tell ParaVisi that it should run play_gol

        for (i = 0; i < data.threads; i++) {
            ret = pthread_create(&tid[i], 0, play_gol, &targs[i]);
        }
        if (ret) { perror("Error pthread_create\n"); exit(1); }
        run_animation(data.handle, data.iters);
    }
    else {
        //checks for a valid output mode: 0, 1, 2
        printf("Invalid output mode: %d\n", data.output_mode);
        printf("Check your game data initialization\n");
        exit(1);
    }


    // Join thread
    for (int i = 0; i < data.threads; i++){
        pthread_join(tid[i], NULL);
    }


    // NOTE: you need to determine how and where to add timing code
    //       in your program to measure the total time to play the given
    //       number of rounds played.
    if (data.output_mode != OUTPUT_VISI) {
        // Computes the total runtime in seconds, including fractional
        // seconds (e.g., 10.5; don't round to 10). for no output and ascii
        //modes
        secs = 0.0;
        ret = gettimeofday(&stop_time, NULL);
        //start time in microseconds
        double micros_st = start_time.tv_usec;
        //convert microseconds to seconds
        micros_st = micros_st/1000000;

        //start time seconds
        double s_st = start_time.tv_sec;
        //adding the micro and second start time to get real start time in secs
        double st = micros_st + s_st;

        //all same as above but for stop time
        double micros_stop = stop_time.tv_usec;
        micros_stop = micros_stop/1000000;

        double s_stop = stop_time.tv_sec;
        double stop = micros_stop + s_stop;
        //total runtime time: stop-start
        secs = stop - st;
       
        
        /* Print the total runtime, in seconds. */
        // NOTE: do not modify these calls to fprintf
        fprintf(stdout, "Total time: %0.3f seconds\n", secs);
        fprintf(stdout, "After %d rounds on %dx%d, the number of live cells is: %d\n\n",
                data.iters, data.rows, data.cols, total_live);
    }

    // clean-up memory before exit
    free(tid);
    free(targs);
    pthread_barrier_destroy(&done);
    if (pthread_mutex_destroy(&my_mutex)) {
        printf("pthread_mutex_destroy error\n");
        exit(1);
    }

    free(data.world);
    free(data.world_copy);
    data.world = NULL;
    data.world_copy = NULL;

    return 0;

}

/******************** Function Prototypes ************************/

/* validate command line
 * data: pointer to gol_data struct to initialize
 * argv: command line args
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode value
 *       argv[3]: number of threads
 *       argv[4]: partition type [row, col]
 *       argv[5]: print mode [yes, no]
 * argc: command line count */
void validation(int argc, char **argv, struct gol_data* data){
    // 
   if (argc != 6) {
        printf("Usage: %s infile.txt output_mode[0,1,2] num_threads[n]"\
              " partition_mode[0,1] print_partition[0,1]\n", argv[0]);
        exit(1);
    }

    /* Initialize game state (all fields in data) from information
     * read from input file */

    int runmode = atoi(argv[2]);
    //spits error if runmode is not one of the valid options
    if (runmode > 3){
        printf("Incorrect run mode entered. options are (0: no visualization,"\
            " 1: ASCII, 2: ParaVisi)\n");
        exit(1);
    }

    int ret = init_game_data_from_args(data, argv);
    if (ret != 0) {
        printf("Initialization error: file %s\n", argv[1]);
        exit(1);
    }

    if(atoi(argv[4]) == 0){
        //row-wise grid cell allocation
        if(atoi(argv[3]) > data->rows){
            printf("Number of threads must be less than number of rows.\n");
            exit(1);
        }
    }

    else if(atoi(argv[4]) == 1){
        //column-wise grid cell allocation
        if(atoi(argv[3]) > data->cols){
            printf("Number of threads must be less than number of colunns.\n");
            exit(1);
        }
    }

    else{
        printf("Please enter 0 or 1 for cell allocation.\n");
        exit(1);
    }

    if ((atoi(argv[5]) != 0) && (atoi(argv[5]) != 1)) {
        printf("Please choose print partition mode [0: no, 1: yes] .\n");
        exit(1);
    }
    else{
        data->print = atoi(argv[5]);
    }
}

/* this function initializes the partition for threads.
it takes care of row wise and column wise partitioning.
    data: pointer to gol_data struct to initialize
    targs: threads info 
    no returns*/
void partition(struct gol_data *data, pthread_t *tid, struct gol_data* targs){
    //base work is the number of rows or columns needed divided evenly across
    //threads, extra work divides the remaining threads across the first
    //few threads
    int base_work, extra_work = 0;

    if (data->divide_mode == 0){
        // col mode
        base_work = data->cols/data->threads;
        if (data->cols%data->threads == 0){
            data->mini_cols = base_work;
        } else{
            data->mini_cols = base_work;
            extra_work = data->cols%data->threads;
        }

        for (int i = 0; i < data->threads; i++){
            targs[i] = *data;
            targs[i].id = i;
            targs[i].mini_rows = data->rows;
        }
        //if there are remaining threads that werent divided evenly across
        //cols
        if (extra_work != 0){
            for (int i = 0; i < extra_work; i++){
                targs[i].mini_cols++;
            } 
        } 

        int index = 0;
        for (int i = 0; i < data->threads; i++){
            targs[i].col_start = index;
            index = index + targs[i].mini_cols;
            targs[i].col_end = targs[i].col_start + targs[i].mini_cols -1 ;
            targs[i].row_start = 0;
            targs[i].row_end = data->rows -1 ;
            
        }

    }

    else {
        // row mode
        base_work = data->rows/data->threads;
        if (data->rows%data->threads == 0){
            data->mini_rows = base_work;
        }
        else {
            data->mini_rows = base_work;
            extra_work = data->rows%data->threads;
        }

        for (int i = 0; i < data->threads; i++){
            targs[i] = *data;
            targs[i].id = i;
            targs[i].mini_cols = data->cols;
        }
        //if there are remaining threads that werent divided evenly across
        //rows
        if (extra_work != 0){
            for (int i = 0; i < extra_work; i++){
                targs[i].mini_rows++;
            }
        }
        
        int index = 0;
        for (int i = 0; i < data->threads; i++){
            targs[i].row_start = index;
            index = index + targs[i].mini_rows;
            targs[i].row_end = targs[i].row_start + targs[i].mini_rows -1 ;
            targs[i].col_start = 0;
            targs[i].col_end = data->rows -1 ;
        }
    }
    

}

/* This function prints partition information, and it prints the 
    thread id, which rows and columns each thread takes care of.

    args: pointer to type void, but will be casted to type gol_data struct 
    the function returns null*/
void *print_stats(void *args) {
    //  based on user input, print the thread information

    struct gol_data *data;
    data = (struct gol_data *)args;


    printf("tid: %5d: rows: %5d:%5d \t(%d) cols: %5d:%5d (%d)\n",\
     data->id, data->row_start, data->row_end, data->mini_rows\
     , data->col_start, data->col_end, data->mini_cols);
    
    
    return NULL;
}

/* initialize the gol game state from command line arguments
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode value
 * data: pointer to gol_data struct to initialize
 * argv: command line args
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode
 * returns: 0 on success, 1 on error
 */
int init_game_data_from_args(struct gol_data *data, char **argv) {

    int temp;

    // (1) declare a FILE * variable
    //     A file pointer is not a pointer like pointers to other
    //     C types: dereferencing it doesn't make any sense.

    FILE *infile;
    infile = fopen(argv[1], "r");

    // (2) open the file for reading and check that open succeeded
    //     (the file name is passed as command line arg)
    if (infile == NULL) { 
        printf("Error: failed to open file: %s\n", argv[1]);
        exit(1);
    }

    // (3) read in some values from the file using fscanf
    //     fscanf is like scanf, except that it takes a FILE *
    //     to the file from which to read as its first argument
    //     it returns the number of items read (or fewer on EOF or error)
    temp = openfile(data, infile);
    
    if (temp ==1){
        printf("error opening file");
        exit(1);
    }

    int runmode = atoi(argv[2]);

    //spits error if runmode is not one of the valid options
    if (runmode > 3){
        printf("Incorrect run mode entered. options are (0: no visualization,"\
            " 1: ASCII, 2: ParaVisi)\n");
        exit(1);
    }

    data->output_mode = runmode;
    
    make_world(data, argv, infile);
    //initializing divide mode and thread number based on user input
    data->divide_mode = atoi(argv[4]);
    data->threads = atoi(argv[3]);

    return 0;
}

/* initialize the world and world copy that will be used to store copies
 * of the world array.
    uses the file name to open the file and save number of coordinate pairs.
    reads in the live coordinate pairs from the file and initializes
    those coordinates with a value of 1, indicating alive
 * param data: pointer to gol_data struct
 * param argv: command line args
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode
 * no returns
 */
void make_world(struct gol_data *data, char **argv, FILE *infile){
    char filename;
    strcpy(&filename, argv[1]);
    
    int ret, sets, i, r, c;
    //reads in number of coordinate pairs from file
    ret = fscanf(infile, "%d", &sets);
    //makes one world array
    data->world = malloc(sizeof(int)*data->rows*data->cols);
    //makes an alternate world array to temporarily store changes
    data->world_copy = malloc(sizeof(int)*data->rows*data->cols);
        //initialize the entire array to be 0
    for(int i=0; i<data->rows; i++){
        for(int j=0; j<data->cols; j++){
            data->world[j*data->rows+i] = 0; 
        }
    }
    
    i = 0;
    ret = 2;

    total_live = sets;
    
    while((i<sets)&&(ret == 2)){
        //reads in coordintates, two values at a time
        //stores in the first value to row, second value to column
        ret = fscanf(infile, "%d%d", &r, &c);
        //if there are less than two integer values that are read in at
        //any time, throw error. (invalid input)
        if (ret != 2){
            printf("Error: Missing input %s\n", &filename);
            exit(1);
        }
        //set coordinate to alive
        data->world[data->cols*r+c] = 1;
        i++;
    }
}

/* This function read in the provided file, and check for proper type inputs
 * param: filename (char): name of file
 *        data (struct gol_data): pointer to struct gol_data
 *        infile (FILE): pointer to type FILE
 * return: 0 if successful
 *         1 if failure
 */
int openfile(struct gol_data *data, FILE *infile){
    
    int ret = fscanf(infile, "%d", &data->rows); // Checking valid type
    if (ret == 0) {
        printf("Improper file format.\n");
        exit(1);
    }
    ret = fscanf(infile, "%d", &data->cols);
    if (ret == 0) {
        printf("Improper file format.\n");
        exit(1);
    }
    ret = fscanf(infile, "%d", &data->iters);
    if (ret == 0) {
        printf("Improper file format.\n");
        exit(1);
    }
    return 0;
}

/**************************************************************/

/* This function is the gol application main loop function:
 *  runs rounds of GOL,
 *    * updates program state for next round (world and total_live)
 *    * performs any animation step based on the output/run mode
 *
 *   param data: pointer to a struct gol_data  initialized with
 *         all GOL game playing state
 *  no return
 */
void *play_gol(void *args) {
    //  at the end of each round of GOL, determine if there is an
    //  animation step to take based on the output_mode,

    int i;
    struct gol_data *data;
    data = (struct gol_data *)args;

    if (data->print){
        print_stats(data);
    }

    //runmode 0 gol: no output
    if (data->output_mode == 0){
        for(i = 0; i < data->iters; i++){

            update_cells(data);
            // Wait for everyone to finish rendering 1 round
            pthread_barrier_wait(&done);

        }
    }

    //runmode 1 gol: ascii output
    //     (a) call system("clear") to clear previous world state from terminal
    //     (b) call print_board function to print current world state
    //     (c) call usleep(SLEEP_USECS) to slow down the animation
    if (data->output_mode == 1){
        for(i = 1; i < data->iters+1; i++){
            update_cells(data);
            pthread_barrier_wait(&done);
            
            // Needs syncing or else jumpled on top of each other

            if (data->id == 0){

                system("clear");
                print_board(data, i);
            }
            pthread_barrier_wait(&done);

            usleep(SLEEP_USECS);
        }   
    }
    
    //   if ParaVis animation:
    //     (a) call your function to update the color3 buffer
    //     (b) call draw_ready(data->handle)
    //     (c) call usleep(SLEEP_USECS) to slow down the animation
    if (data->output_mode == 2){
        for(i = 1; i <= data->iters; i++){
            update_cells(data);
            pthread_barrier_wait(&done);

            update_colors(data);
            pthread_barrier_wait(&done);

            draw_ready(data->handle);
            usleep(SLEEP_USECS);
        }
    }
    
    return NULL;
}


/* This function updates the cells, checking if they are alive or
 * dead based on the number of neighbors the cell has. this function
 * checks if the cells are alive in the world, but updates their
 * live or dead status in the world_copy, as to not disrupt future 
 * cells that still must be checked in world. (disrupt by prematurely
 * changing life status)
 * param data: pointer to a struct gol_data  initialized with
 *         all GOL game playing state
 *  no returns
 */
void update_cells(struct gol_data *data){
    
    int *temp;
    
    int i, j, num_neighbors;

    //change in live cell count
    int mylivecount = 0, mylivecount_now = 0, delta_mylivecount;

    for (i = data->row_start; i <= data->row_end; i++){
        for (j = data->col_start; j <= data->col_end; j++){
            if (data->world[j*data->rows+i] == 1){
                mylivecount++;
            }
    }
    }

    //iterate through all of the cells in world array
    for (i = data->row_start; i <= data->row_end; i++){
        for (j = data->col_start; j <= data->col_end; j++){
            
            num_neighbors = check_neighbors(data, i, j);
            //condition that makes the cell alive: checking world

            if ((num_neighbors == 3) || ((num_neighbors == 2) && \
                (data->world[j*data->rows+i] == 1))){
                
                //updates live or dead in the world_copy
                data->world_copy[j*data->rows+i] = 1;
                

            }
            else{
                data->world_copy[j*data->rows+i] = 0;
            }
        }
    }

    for (i = data->row_start; i <= data->row_end; i++){
        for (j = data->col_start; j <= data->col_end; j++){
            if (data->world_copy[j*data->rows+i] == 1){
                mylivecount_now++;
            }
        }
    }

    delta_mylivecount = mylivecount_now - mylivecount;
    

    pthread_mutex_lock(&my_mutex);
    total_live += delta_mylivecount;
    pthread_mutex_unlock(&my_mutex);

    //swap pointers to both worlds after each round
    temp = data->world;
    data->world = data->world_copy;
    data->world_copy = temp;

}


/* This function iterates through all of the neighbors of the cell that
 * passed in, starting with the upper left neighbor, and circling all
 * 8 neighbors in clockwise fashion. it checks if each neighboring cell
 * is alive, and if it is alive, it adds one to the count of number of 
 * neighbors.
 * param data: pointer to a struct gol_data  initialized with
 *         all GOL game playing state
 * param int row: the value of current cell's row
 * param int col: the value of current cell's column
 * returns the number of neighbors the current cell has
 */
int check_neighbors(struct gol_data *data, int row, int col){
    //initiaize neighbor count to zero
    int count = 0;
    //start with the upper left neighbor
    row--;
    col--;
    //these cell transformations take care of edge and corner cases, so 
    //neighbors automatically change to cells that are on opposite sides of
    //the game board
    int r_trans = (row+data->rows) % data->rows;
    int c_trans = (col+data->cols) % data->cols;

    //if the upper left cell is alive, add 1 to count
    if (data->world[c_trans*data->rows+r_trans]==1){
        count++;
    }
    //iterate through remaining neighbors 1-7
    for(int i=1; i<8; i++){
        //look to the neighbor one to the right
        if (i<3){
            col++;
            //do transformation for the new coordinatex to take care of edges
            r_trans = (row+data->rows) % data->rows;
            c_trans = (col+data->cols) % data->cols;

            //add one to live neighbor count if neighbor is alive
            if (data->world[c_trans*data->rows+r_trans]==1){
            count++;
            }
        }
        if((i==3)||(i==4)){
            //move down one row to check new neighbor
            row++;
            //do transformation for the new coordinatex to take care of edges
            r_trans = (row+data->rows) % data->rows;
            c_trans = (col+data->cols) % data->cols;

            //add one to live neighbor count if neighbor is alive
            if (data->world[c_trans*data->rows+r_trans]==1){
            count++;
            }
        }
        if((i==5)||(i==6)){
            //move left one row to check new neighbor
            col--;
            //do transformation for the new coordinatex to take care of edges
            r_trans = (row+data->rows) % data->rows;
            c_trans = (col+data->cols) % data->cols;

            //add one to live neighbor count if neighbor is alive
            if (data->world[c_trans*data->rows+r_trans]==1){
            count++;
            }
        }
        if((i==7)){
            //move up one row to check final neighbor
            row--;
            //do transformation for the new coordinatex to take care of edges
            r_trans = (row+data->rows) % data->rows;
            c_trans = (col+data->cols) % data->cols;

            //add one to live neighbor count if neighbor is alive
            if (data->world[c_trans*data->rows+r_trans]==1){
            count++;
            }
        }
    }
    return count;
}


/* This function describes how the pixels in the image buffer should be
 * colored based on the data in the grid.
 * param data: pointer to a struct gol_data  initialized with
 *         all GOL game playing state
 *  no returns
 */
void update_colors(struct gol_data *data) {

    int i, j, r, c, index, buff_i;
    color3 *buff;

    buff = data->image_buff;  // just for readability
    r = data->rows;
    c = data->cols;

    for (i = data->row_start; i <= data->row_end; i++) {
        for (j = data->col_start; j <= data->col_end; j++) {

            index = i*c + j;
            // translate row index to y-coordinate value because in
            // the image buffer, (r,c)=(0,0) is the _lower_ left but
            // in the grid, (r,c)=(0,0) is _upper_ left.
            buff_i = (r - (i+1))*c + j;

            // update animation buffer
            if (data->world[index] == 0) {
                buff[buff_i] = colors[data->id%8];
            } else if (data->world[index] == 1) {
                buff[buff_i] = c3_black;
            }
        }
    }
}
/**************************************************************/
/* Print the board to the terminal.
 *   data: gol game specific data
 *   round: the current round number
 * no return value
 *
 * NOTE: You may add extra printfs if you'd like, but please
 *       leave these fprintf calls exactly as they are to make
 *       grading easier!

 */
void print_board(struct gol_data *data, int round) {

    int i, j;

    /* Print the round number. */
    fprintf(stderr, "Round: %d\n", round);

    for (i = 0; i < data->rows; ++i) {
        for (j = 0; j < data->cols; ++j) {
            //if cell is alive
            if (data->world[i*data->rows+j] == 1){
                fprintf(stderr, " @");
            }
            else{
                //otherwise
                fprintf(stderr, " .");
            }
            
        }
        fprintf(stderr, "\n");
    }


    /* Print the total number of live cells. */
    fprintf(stderr, "Live cells: %d\n\n", total_live);
}

/**************************************************************/
/**************************************************************/
/***** START: DO NOT MODIFY THIS CODE *****/
/* initialize ParaVisi animation */
int setup_animation(struct gol_data* data) {
    /* connect handle to the animation */
    int num_threads = data->threads;
    data->handle = init_pthread_animation(num_threads, data->rows,
            data->cols, visi_name);

    if (data->handle == NULL) {
        printf("ERROR init_pthread_animation\n");
        exit(1);
    }
    // get the animation buffer
    data->image_buff = get_animation_buffer(data->handle);
    if(data->image_buff == NULL) {
        printf("ERROR get_animation_buffer returned NULL\n");
        exit(1);
    }
    return 0;
}

/* sequential wrapper functions around ParaVis library functions */
void (*mainloop)(struct gol_data *data);

void* seq_do_something(void * args){
    mainloop((struct gol_data *)args);
    return 0;
}
