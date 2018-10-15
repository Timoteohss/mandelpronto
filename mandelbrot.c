#include <stdio.h>
#include <mpi.h>
#include <math.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#include "gui.h"

#define DEBUG 0
#define DEBUG_ROW 10

#define prob_space 2

#define WORK 1
#define DATA 2
#define HALT 0

typedef struct complex_number {
    double real;
    double imag;
} complex_number;

int master_mandelbrot(int nworkers, int width, int height,
        double real_min, double real_max,
        double imag_min, double imag_max,
        int maxiter);

int worker_mandelbrot(int myid, int width, int height,
        double real_min, double real_max,
        double imag_min, double imag_max,
        int maxiter); 

int main(int argc, char *argv[]) 
{
    int nprocs;
    int myid;
    int returnval;
    int maxiter;
    double real_min = -prob_space;
    double real_max = prob_space;
    double imag_min = -prob_space;
    double imag_max = prob_space;
    int width = 600;
    int height = 600;

    if ( MPI_Init(&argc, &argv) != MPI_SUCCESS ) 
    {
        fprintf(stderr, "MPI initialization error. \n");
        exit(EXIT_FAILURE);
    }

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);

    if( nprocs < 2 )
    {
        fprintf(stderr, "Numero de processes deve ser pelo menos 2.\n");
        MPI_Finalize();
        exit(EXIT_FAILURE);
    }

    //Pega argumentos

    if(argc == 2) 
    {
        sscanf(argv[1], "%d", &maxiter);
        //fprintf(stderr, "Recebi: %d, meu processo é o de número %d.\n", maxiter, myid);
    }
    else
    {
        if(argc > 2)
        {
          //coisar centro
        }
    }

    if( myid == 0 )
    {
        returnval = master_mandelbrot(nprocs-1, 
                    width, height, 
                    real_min, real_max, 
                    imag_min, imag_max, 
                    maxiter);
    }
    else
    {
        returnval = worker_mandelbrot(myid, 
                    width, height,
                    real_min, real_max,
                    imag_min, imag_max,
                    maxiter);
    }

    MPI_Finalize();
    return returnval;
}

int master_mandelbrot(int nworkers, int width, int height,
        double real_min, double real_max,
        double imag_min, double imag_max,
        int maxiter) 
{
    Display *display;
    Window win;
    GC gc;

    long min_color = 0, max_color = 0;

    int setup_return = setup(width, height, &display, &win, 
                    &gc, &min_color, &max_color);
    if (setup_return != EXIT_SUCCESS) 
    {
        fprintf(stderr, "Nao pude abrir a janela\n");
        fflush(stderr);
    }

    double started, finished;
    started = MPI_Wtime();

    MPI_Bcast(&min_color, 1, MPI_LONG, 0, MPI_COMM_WORLD);    
    MPI_Bcast(&max_color, 1, MPI_LONG, 0, MPI_COMM_WORLD);

    int nrow = 0;
    int working = 0;
    int proc = 0;


    for (proc ; proc < nworkers; ++proc) 
    {
        MPI_Send(&nrow, 1, MPI_INT, proc+1, WORK, MPI_COMM_WORLD);
        ++nrow;
        ++working;
    }

    MPI_Status status;
    long *data_msg = malloc((width+1) * sizeof(*data_msg));
    int id;


    while (working > 0)
    {
        MPI_Recv(data_msg, width + 1, MPI_LONG, MPI_ANY_SOURCE, DATA, MPI_COMM_WORLD, &status);
        --working;
        id = status.MPI_SOURCE;

        if(nrow < height)
        {
            MPI_Send(&nrow, 1, MPI_INT, id, WORK, MPI_COMM_WORLD);
            ++nrow;
            ++working;
        }
        else
        {
            MPI_Send(&nrow,0,MPI_INT,id,HALT,MPI_COMM_WORLD);
        }

        int trow = data_msg[0];
        for (int col = 0; col < width; ++col)
        {
            if(setup_return == EXIT_SUCCESS)
            {
                XSetForeground(display, gc, data_msg[col+1]);
                XDrawPoint(display, win, gc, col, trow);
            }
        }
    }

    if (setup_return == EXIT_SUCCESS)
    {
        XFlush(display);
    }

    finished = MPI_Wtime();

    fprintf(stdout,"Executado em: %g segundos.\n", finished - started);

    XEvent report;
    XSelectInput(display, win, KeyPressMask | ButtonPressMask);

    while(True) {
        XNextEvent(display, &report);
        switch(report.type){
            case KeyPress:
                break;
        }
    }

    return EXIT_SUCCESS;
}

int worker_mandelbrot(int nworkers, int width, int height,
        double real_min, double real_max,
        double imag_min, double imag_max,
        int maxiter) 
{
    MPI_Status status;
    int row;
    long min_color, max_color;
    double scale_real, scale_imag, scale_color;
    long *data_msg = malloc((width+1) * sizeof(*data_msg));

    MPI_Bcast(&min_color, 1, MPI_LONG, 0, MPI_COMM_WORLD);
    MPI_Bcast(&max_color, 1, MPI_LONG, 0, MPI_COMM_WORLD);

    scale_real = (double) (real_max - real_min) / (double) width;
    scale_imag = (double) (imag_max - imag_min) / (double) height;
    scale_color = (double) (max_color - min_color) / (double) (maxiter - 1);

    while ( ((MPI_Recv(&row, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status)) == MPI_SUCCESS ) &&
            (status.MPI_TAG == WORK ) )
    {
        data_msg[0] = row;

        for( int col = 0 ; col < width; ++col)
        {
            complex_number z, c;

            z.real = z.imag = 0;

            // Calcula a escala do display para encaixar a imagem na região
            c.real = real_min + ((double) (height-1-row) * scale_real);
            c.imag = imag_min + ((double) (col) * scale_imag);
            /*  OBS: height-1-row para o eixo y 
            *   mostrar os maiores valores de y na parte superior
            */
            
	    // Calcular z0,z1... até iteração maxima
            int iter = 0;
            double lenghtsq, temp;

            do 
            {    
                temp = z.real*z.real - z.imag*z.imag + c.real;
                
		z.imag = 2*z.real*z.imag + c.imag;
                z.real = temp;
                lenghtsq = z.real*z.real + z.imag*z.imag;

                ++iter;
                


            } while (lenghtsq < (prob_space*prob_space) && iter < maxiter);

            //Calcula a escala de cores
            long color = (long) ((iter-1) * scale_color) + min_color;
            data_msg[col+1] = color;
        }
        MPI_Send(data_msg, width+1, MPI_LONG, 0, DATA, MPI_COMM_WORLD);
    }
    free(data_msg);
    return EXIT_SUCCESS;
}
