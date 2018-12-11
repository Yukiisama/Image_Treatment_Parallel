
#include "compute.h"
#include "debug.h"
#include "global.h"
#include "graphics.h"
#include "ocl.h"
#include "pthread_barrier.h"

#include <pthread.h>
#include <stdbool.h>

static inline unsigned invert_pixel (unsigned c)
{
  return ((unsigned)0xFFFFFFFF - c) | 0xFF;
}


///////////////////////////// Version séquentielle simple (seq)

// Renvoie le nombre d'itérations effectuées avant stabilisation, ou 0
unsigned invert_compute_seq (unsigned nb_iter)
{
  for (unsigned it = 1; it <= nb_iter; it++) {

    for (int i = 0; i < DIM; i++)
      for (int j = 0; j < DIM; j++)
        cur_img (i, j) = invert_pixel (cur_img (i, j));
  }

  return 0;
}


///////////////////////////// Version thread simple



static unsigned nb_threads = 2;
static unsigned nb_iterp;
void * invert_compute_seq_thread (void * argv)
{
  int k = (intptr_t) argv;
  int start = (DIM/nb_threads) * k;
  int end = (DIM/nb_threads) * (k + 1);
  for (unsigned it = 1; it <= nb_iterp; it++) {
  
    for (int i = start; i < end; i++)
      for (int j = 0; j < DIM; j++)
        cur_img (i, j) = invert_pixel (cur_img (i, j));
  }

  return 0;
}


unsigned invert_compute_thread (unsigned nb_iter)
{
  char *str = getenv ("OMP_NUM_THREADS");

  if (str != NULL)
    nb_threads = atoi (str);
  else
    nb_threads = get_nb_cores ();
  nb_iterp = nb_iter;
  pthread_t thread_tab[nb_threads];
  for (int i = 0; i<nb_threads;i++){
    thread_tab[i] = 0; 
    pthread_create(&thread_tab[i],NULL,invert_compute_seq_thread,(void *) (intptr_t) i);
  }
  for (int i = 0; i<nb_threads;i++){
    pthread_join(thread_tab[i],NULL);
  }

  
  
  return 0;
}
