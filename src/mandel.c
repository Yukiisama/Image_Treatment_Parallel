
#include "compute.h"
#include "debug.h"
#include "global.h"
#include "graphics.h"
#include "ocl.h"
#include "pthread_barrier.h"

#include <stdbool.h>

#define MAX_ITERATIONS 4096
#define ZOOM_SPEED -0.01

static unsigned iteration_to_color(unsigned iter)
{
  unsigned r = 0, g = 0, b = 0;

  if (iter < MAX_ITERATIONS)
  {
    if (iter < 64)
    {
      r = iter * 2; /* 0x0000 to 0x007E */
    }
    else if (iter < 128)
    {
      r = (((iter - 64) * 128) / 126) + 128; /* 0x0080 to 0x00C0 */
    }
    else if (iter < 256)
    {
      r = (((iter - 128) * 62) / 127) + 193; /* 0x00C1 to 0x00FF */
    }
    else if (iter < 512)
    {
      r = 255;
      g = (((iter - 256) * 62) / 255) + 1; /* 0x01FF to 0x3FFF */
    }
    else if (iter < 1024)
    {
      r = 255;
      g = (((iter - 512) * 63) / 511) + 64; /* 0x40FF to 0x7FFF */
    }
    else if (iter < 2048)
    {
      r = 255;
      g = (((iter - 1024) * 63) / 1023) + 128; /* 0x80FF to 0xBFFF */
    }
    else
    {
      r = 255;
      g = (((iter - 2048) * 63) / 2047) + 192; /* 0xC0FF to 0xFFFF */
    }
  }
  return (r << 24) | (g << 16) | (b << 8) | 255 /* alpha */;
}

// Cadre initial
#if 0
// Config 1
static float leftX = -0.744;
static float rightX = -0.7439;
static float topY = .146;
static float bottomY = .1459;
#endif

#if 1
// Config 2
static float leftX = -0.2395;
static float rightX = -0.2275;
static float topY = .660;
static float bottomY = .648;
#endif

#if 0
// Config 3
static float leftX = -0.13749;
static float rightX = -0.13715;
static float topY = .64975;
static float bottomY = .64941;
#endif

static float xstep;
static float ystep;

static void zoom(void)
{
  float xrange = (rightX - leftX);
  float yrange = (topY - bottomY);

  leftX += ZOOM_SPEED * xrange;
  rightX -= ZOOM_SPEED * xrange;
  topY -= ZOOM_SPEED * yrange;
  bottomY += ZOOM_SPEED * yrange;

  xstep = (rightX - leftX) / DIM;
  ystep = (topY - bottomY) / DIM;
}

void mandel_init()
{
  xstep = (rightX - leftX) / DIM;
  ystep = (topY - bottomY) / DIM;
}

static unsigned compute_one_pixel(int i, int j)
{
  float xc = leftX + xstep * j;
  float yc = topY - ystep * i;
  float x = 0.0, y = 0.0; /* Z = X+I*Y */

  int iter;

  // Pour chaque pixel, on calcule les termes d'une suite, et on
  // s'arrête lorsque |Z| > 2 ou lorsqu'on atteint MAX_ITERATIONS
  for (iter = 0; iter < MAX_ITERATIONS; iter++)
  {
    float x2 = x * x;
    float y2 = y * y;

    /* Stop iterations when |Z| > 2 */
    if (x2 + y2 > 4.0)
      break;

    float twoxy = (float)2.0 * x * y;
    /* Z = Z^2 + C */
    x = x2 - y2 + xc;
    y = twoxy + yc;
  }

  return iter;
}

///////////////////////////// Version séquentielle simple (seq)

static void traiter_tuile(int i_d, int j_d, int i_f, int j_f)
{
  PRINT_DEBUG('c', "tuile [%d-%d][%d-%d] traitée\n", i_d, i_f, j_d, j_f);

  for (int i = i_d; i <= i_f; i++)
    for (int j = j_d; j <= j_f; j++)
      cur_img(i, j) = iteration_to_color(compute_one_pixel(i, j));
}

// Renvoie le nombre d'itérations effectuées avant stabilisation, ou 0
unsigned mandel_compute_seq(unsigned nb_iter)
{
  for (unsigned it = 1; it <= nb_iter; it++)
  {

    // On traite toute l'image en une seule fois
    traiter_tuile(0, 0, DIM - 1, DIM - 1);
    zoom();
  }

  return 0;
}

///////////////////////////// Version thread naïve par bloc de lignes contigues

static int indice = 0;
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
int distributeur_suivant()
{
  int i;
  pthread_mutex_lock(&mut);
  indice++;
  i = indice;
  pthread_mutex_unlock(&mut);
  return i;
}

static unsigned nb_threads = 2;
static unsigned nb_iterp;
pthread_barrier_t b;

void *thread_dyn(void *argv)
{
  int line;
  int me = (intptr_t)argv;
  for (unsigned it = 1; it <= nb_iterp; it++)
  {

    while ((line = distributeur_suivant()) < DIM)
      traiter_tuile(line, 0, line, DIM - 1);
    pthread_barrier_wait(&b);
    if (me == 0)
    {
      zoom();
      indice = 0;
    }
    pthread_barrier_wait(&b);
  }

  return 0;
}

void *thread_cyclic(void *argv)
{

  int me = (intptr_t)argv;

  for (unsigned it = 1; it <= nb_iterp; it++)
  {
    for (int i = me; i < DIM; i += nb_threads)
      traiter_tuile(i, 0, i, DIM - 1);
    pthread_barrier_wait(&b);
    if (me == 0)
      zoom();
    pthread_barrier_wait(&b);
  }

  return 0;
}

void *mandel_compute_thread_s(void *argv)
{
  int k = (intptr_t)argv;
  int start = (DIM / nb_threads) * k;
  int end = (DIM / nb_threads) * (k + 1);
  for (unsigned it = 1; it <= nb_iterp; it++)
  {

    traiter_tuile(start, 0, end - 1, DIM - 1);

    pthread_barrier_wait(&b);
    if (k == 0)
      zoom();
    pthread_barrier_wait(&b);
  }

  return 0;
}

unsigned mandel_compute_thread(unsigned nb_iter)
{
  char *str = getenv("OMP_NUM_THREADS");
  pthread_mutex_init(&mut, NULL);
  if (str != NULL)
    nb_threads = atoi(str);
  else
    nb_threads = get_nb_cores();
  pthread_barrier_init(&b, NULL, nb_threads);
  nb_iterp = nb_iter;
  pthread_t thread_tab[nb_threads];
  for (int i = 0; i < nb_threads; i++)
  {
    thread_tab[i] = 0;
    //pthread_create(&thread_tab[i],NULL,mandel_compute_thread_s ,(void *) (intptr_t) i);
    //pthread_create(&thread_tab[i],NULL,thread_cyclic ,(void *) (intptr_t) i);
    pthread_create(&thread_tab[i], NULL, thread_dyn, (void *)(intptr_t)i);
  }
  for (int i = 0; i < nb_threads; i++)
  {
    pthread_join(thread_tab[i], NULL);
  }

  return 0;
}