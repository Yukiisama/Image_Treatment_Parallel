
#include "compute.h"
#include "debug.h"
#include "global.h"
#include "graphics.h"
#include "ocl.h"
#include "pthread_barrier.h"

#include <stdbool.h>

///////////////////////////// Version séquentielle simple (seq)

static unsigned pixel_mean (int y, int x)
{
  unsigned r = 0, g = 0, b = 0, a = 0, n = 0;

  int i_d = (y > 0) ? y - 1 : y;
  int i_f = (y < DIM - 1) ? y + 1 : y;
  int j_d = (x > 0) ? x - 1 : x;
  int j_f = (x < DIM - 1) ? x + 1 : x;

  for (int i = i_d; i <= i_f; i++)
    for (int j = j_d; j <= j_f; j++) {
      unsigned c = cur_img (i, j);
      r += (c >> 24) & 255;
      g += (c >> 16) & 255;
      b += (c >> 8) & 255;
      a += c & 255;
      n += 1;
    }

  r /= n;
  g /= n;
  b /= n;
  a /= n;

  return (r << 24) | (g << 16) | (b << 8) | a;
}

// Renvoie le nombre d'itérations effectuées avant stabilisation, ou 0
unsigned blur_compute_seq (unsigned nb_iter)
{
  for (unsigned it = 1; it <= nb_iter; it++) {

    for (int i = 0; i < DIM; i++)
      for (int j = 0; j < DIM; j++)
        next_img (i, j) = pixel_mean (i, j);

    swap_images ();
  }

  return 0;
}


///////////////////////////// Version thread simple



static unsigned nb_threads = 2;
static unsigned nb_iterp;
pthread_barrier_t b;
void * blur_compute_thread_s(void * argv)
{
  int k = (intptr_t) argv;
  int start = (DIM/nb_threads) * k;
  int end = (DIM/nb_threads) * (k + 1);
  for (unsigned it = 1; it <= nb_iterp; it++) {
  
    for (int i = start; i < end; i++)
      for (int j = 0; j < DIM; j++)
        next_img (i, j) = pixel_mean (i, j);

  pthread_barrier_wait(&b); // ON ATTEND CHAQUE THREAD FINI
  if( k == 0)
    swap_images (); // on a 2 images une ou on lit l'autre on ecrit
  pthread_barrier_wait(&b); // ON ATTEND CHAQUE THREAD FINI
  }

  return 0;
}


unsigned blur_compute_thread(unsigned nb_iter)
{
  char *str = getenv ("OMP_NUM_THREADS");
  if (str != NULL)
    nb_threads = atoi (str);
  else
    nb_threads = get_nb_cores ();
  nb_iterp = nb_iter;
  pthread_barrier_init(&b,NULL,nb_threads);
  pthread_t thread_tab[nb_threads];
  for (int i = 0; i<nb_threads;i++){
    thread_tab[i] = 0; 
    pthread_create(&thread_tab[i],NULL,blur_compute_thread_s,(void *) (intptr_t) i);
  }
  for (int i = 0; i<nb_threads;i++){
    pthread_join(thread_tab[i],NULL);
  }

  
  
  return 0;
}