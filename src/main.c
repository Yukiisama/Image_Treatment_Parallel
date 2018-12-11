#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <hwloc.h>

#include <SDL.h>

#include "compute.h"
#include "constants.h"
#include "debug.h"
#include "error.h"
#include "global.h"
#include "graphics.h"
#include "ocl.h"

// Returns duration in µsecs
#define TIME_DIFF(t1, t2)                                                      \
  ((t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_usec - t1.tv_usec))

static char *progname = NULL;

int max_iter          = 0;
unsigned refresh_rate = 1;
unsigned GRAIN        = 8;

static  hwloc_topology_t topology;
static  unsigned nb_cores = 1;

void_func_t the_first_touch = NULL;
void_func_t the_init        = NULL;
draw_func_t the_draw        = NULL;
void_func_t the_finalize    = NULL;
int_func_t the_compute      = NULL;
void_func_t the_refresh_img = NULL;

char *version        = DEFAULT_VARIANT;
unsigned opencl_used = 0;

unsigned get_nb_cores (void)
{
  return nb_cores;
}

static void update_refresh_rate (int p)
{
  static int tab_refresh_rate[] = {1, 2, 5, 10, 100, 1000, 10000, 100000};
  static int i_refresh_rate     = 0;

  if ((i_refresh_rate == 0 && p < 0) || (i_refresh_rate == 7 && p > 0))
    return;

  i_refresh_rate += p;
  refresh_rate = tab_refresh_rate[i_refresh_rate];
  printf ("\nrefresh rate = %d \n", refresh_rate);
}

static void usage (int val)
{
  fprintf (stderr, "Usage: %s [option]\n", progname);
  fprintf (stderr, "option can be:\n");
  fprintf (stderr,
           "\t-k\t| --kernel <name>\t: override KERNEL environment variable\n");
  fprintf (stderr,
           "\t-n\t| --no-display\t\t: avoid graphical display overhead\n");
  fprintf (stderr, "\t-l\t| --load-image <file>\t: use PNG image <file>\n");
  fprintf (
      stderr,
      "\t-p\t| --param <string>\t: pass parameter <string> to draw function\n");
  fprintf (stderr, "\t-s\t| --size <DIM>\t\t: use image of size DIM x DIM\n");
  fprintf (stderr, "\t-i\t| --iterations <n>\t: stop after n iterations\n");
  fprintf (stderr,
           "\t-r\t| --refresh-rate <N>\t: display only 1/Nth of images\n");
  fprintf (stderr, "\t-d\t| --debug-flags <flags>\t: enable debug messages\n");
  fprintf (stderr,
           "\t-v\t| --version <name>\t: select version <name> of algorithm\n");
  fprintf (stderr, "\t-o\t| --ocl\t\t\t: use OpenCL version\n");
  fprintf (stderr,
           "\t-ft\t| --first-touch\t\t: touch memory on different cores\n");
  fprintf (stderr, "\t-g\t| --grain\t\t: use grain x grain tiles\n");
  fprintf (stderr, "\t-h\t| --help\t\t: display help\n");

  exit (val);
}

static void filter_args (int *argc, char *argv[])
{
  progname = argv[0];

  // Filter args
  //
  argv++;
  (*argc)--;
  while (*argc > 0) {
    if (!strcmp (*argv, "--no-vsync") || !strcmp (*argv, "-nvs")) {
      vsync = 0;
    } else if (!strcmp (*argv, "--no-display") || !strcmp (*argv, "-n")) {
      display = 0;
    } else if (!strcmp (*argv, "--help") || !strcmp (*argv, "-h")) {
      usage (0);
    } else if (!strcmp (*argv, "--first-touch") || !strcmp (*argv, "-ft")) {
      do_first_touch = 1;
    } else if (!strcmp (*argv, "--param") || !strcmp (*argv, "-p")) {
      if (*argc == 1) {
        fprintf (stderr, "Error: parameter string missing\n");
        usage (1);
      }
      (*argc)--;
      argv++;
      draw_param = *argv;
    } else if (!strcmp (*argv, "--ocl") || !strcmp (*argv, "-o")) {
      opencl_used = 1;
    } else if (!strcmp (*argv, "--kernel") || !strcmp (*argv, "-k")) {
      if (*argc == 1) {
        fprintf (stderr, "Error: kernel name missing\n");
        usage (1);
      }
      (*argc)--;
      argv++;
      setenv ("KERNEL", *argv, 1);
    } else if (!strcmp (*argv, "--load-image") || !strcmp (*argv, "-li") ||
               !strcmp (*argv, "-l")) {
      if (*argc == 1) {
        fprintf (stderr, "Error: filename missing\n");
        usage (1);
      }
      (*argc)--;
      argv++;
      pngfile = *argv;
    } else if (!strcmp (*argv, "--size") || !strcmp (*argv, "-s")) {
      if (*argc == 1) {
        fprintf (stderr, "Error: DIM missing\n");
        usage (1);
      }
      (*argc)--;
      argv++;
      DIM = atoi (*argv);
    } else if (!strcmp (*argv, "--grain") || !strcmp (*argv, "-g")) {
      if (*argc == 1) {
        fprintf (stderr, "Error: DIM missing\n");
        usage (1);
      }
      (*argc)--;
      argv++;
      GRAIN = atoi (*argv);
    } else if (!strcmp (*argv, "--version") || !strcmp (*argv, "-v")) {

      if (*argc == 1) {
        fprintf (stderr, "Error: version number missing\n");
        usage (1);
      }
      (*argc)--;
      argv++;
      if (!strcmp (*argv, "ocl"))
        opencl_used = 1;
      version = *argv;
    } else if (!strcmp (*argv, "--iterations") || !strcmp (*argv, "-i")) {
      if (*argc == 1) {
        fprintf (stderr, "Error: N missing\n");
        usage (1);
      }
      (*argc)--;
      argv++;
      max_iter = atoi (*argv);
    } else if (!strcmp (*argv, "--refresh-rate") || !strcmp (*argv, "-r")) {
      if (*argc == 1) {
        fprintf (stderr, "Error: N missing\n");
        usage (1);
      }
      (*argc)--;
      argv++;
      refresh_rate = atoi (*argv);
    } else if (!strcmp (*argv, "--debug-flags") || !strcmp (*argv, "-d")) {
      if (*argc == 1) {
        fprintf (stderr, "Error: flag list missing\n");
        usage (1);
      }
      (*argc)--;
      argv++;
      debug_init (*argv);
    } else {
      fprintf (stderr, "Error: unknown flag %s\n", *argv);
      usage (1);
    }

    (*argc)--;
    argv++;
  }
}

void *bind_it (char *kernel, char *s, char *version, int print_error)
{
  char buffer[1024];
  void *fun = NULL;
  sprintf (buffer, "%s_%s_%s", kernel, s, version);
  fun = dlsym (DLSYM_FLAG, buffer);
  if (fun != NULL)
    printf (" %s", buffer);
  else {
    sprintf (buffer, "%s_%s", kernel, s);
    fun = dlsym (DLSYM_FLAG, buffer);
    if (fun != NULL)
      printf (" %s", buffer);
    else if (print_error)
      exit_with_error ("Cannot resolve symbol %s\n", buffer);
  }
  return fun;
}

static void bind_functions (void)
{
  char *kernel = NULL;

  kernel = getenv ("KERNEL");
  if (kernel == NULL)
    kernel = DEFAULT_KERNEL;

  printf ("Using kernel [%s], variant [%s]\n", kernel, version);
  printf ("Bindings : ");

  the_compute = bind_it (kernel, "compute", version, !opencl_used);

  if (the_compute == NULL) {
    if (opencl_used) {
      fprintf (stderr, "Warning: Cannot find specialized ocl routine\n");
      fprintf (stderr,
               "         => falling back to generic OpenCL kernel launcher!\n");
      the_compute = ocl_compute;
    }
  }

  the_init        = bind_it (kernel, "init", version, 0);
  the_draw        = bind_it (kernel, "draw", draw_param, 0);
  the_finalize    = bind_it (kernel, "finalize", version, 0);
  the_refresh_img = bind_it (kernel, "refresh_img", version, 0);

  if (!opencl_used) {
    the_first_touch = bind_it (kernel, "ft", version, do_first_touch);
  }

  printf ("\n");
}

int main (int argc, char **argv)
{
  int stable     = 0;
  int iterations = 0;
  unsigned step  = 0;

  filter_args (&argc, argv);

  /* Allocate and initialize topology object. */
  hwloc_topology_init (&topology);

  /* Perform the topology detection. */
  hwloc_topology_load (topology);

  nb_cores = hwloc_get_nbobjs_by_type (topology, HWLOC_OBJ_PU);
  PRINT_DEBUG ('t', "%d-core machine detected\n", nb_cores);

  bind_functions ();

  if (the_init != NULL)
    the_init ();

  graphics_init ();
  // Now we know the value of DIM

  if (opencl_used) {

    ocl_init ();
    ocl_send_image (image);
  }

  if (graphics_display_enabled ()) {
    // version graphique

    unsigned long temps = 0;
    struct timeval t1, t2;

    if (opencl_used)
      graphics_share_texture_buffers ();

    graphics_refresh ();

    for (int quit = 0; !quit;) {

      if (debug_enabled ('p')) {
        printf ("=== itération %d ===\n", iterations);
        step = 1;
      }

      // Récupération éventuelle des événements clavier, souris, etc.
      do {
        SDL_Event evt;

        while (SDL_PollEvent (&evt)) {

          switch (evt.type) {
          case SDL_QUIT:
            quit = 1;
            break;
          case SDL_KEYDOWN:
            // Si l'utilisateur appuie sur une touche
            switch (evt.key.keysym.sym) {
            case SDLK_ESCAPE:
              if (!stable)
                printf ("\nSortie forcée à l'itération %d\n", iterations);
              quit = 1;
              break;
            case SDLK_SPACE:
              step = 1 - step;
              break;

            case SDLK_DOWN:
              update_refresh_rate (-1);
              break;

            case SDLK_UP:
              update_refresh_rate (1);
              break;

            default:;
            }
            break;

          default:;
          }
        }
      } while (step && !quit);

      if (!stable && !quit) {
        if (max_iter && iterations >= max_iter) {
          if (debug_enabled ('t'))
            printf ("\nArrêt après %d itérations (durée %ld.%03ld)\n",
                    iterations, temps / 1000, temps % 1000);
          else
            printf ("Arrêt après %d itérations\n", max_iter);
          stable = 1;
          graphics_refresh ();
        } else {
          int n;

          if (debug_enabled ('t')) {
            long duree_iteration;

            gettimeofday (&t1, NULL);
            n = the_compute (refresh_rate);
            if (opencl_used)
              ocl_wait ();
            gettimeofday (&t2, NULL);

            duree_iteration = TIME_DIFF (t1, t2);
            temps += duree_iteration;
            int nbiter = (n > 0 ? n : refresh_rate);
            fprintf (stderr,
                     "\r dernière iteration  %ld.%03ld -  temps moyen par "
                     "itération : %ld.%03ld ",
                     duree_iteration / nbiter / 1000,
                     (duree_iteration / nbiter) % 1000,
                     temps / 1000 / (nbiter + iterations),
                     (temps / (nbiter + iterations)) % 1000);
          } else
            n = the_compute (refresh_rate);

          if (n > 0) {
            iterations += n;
            stable = 1;
            if (debug_enabled ('t'))
              printf ("\nCalcul terminé en %d itérations (durée %ld.%03ld)\n",
                      iterations, temps / 1000, (temps) % 1000);
            else
              printf ("Calcul terminé en %d itérations\n", iterations);

          } else
            iterations += refresh_rate;

          if (the_refresh_img)
            the_refresh_img ();
          graphics_refresh ();
        }
      }
    }
  } else {
    // Version non graphique
    unsigned long temps;
    struct timeval t1, t2;
    int n;

    if (max_iter)
      refresh_rate = max_iter;

    gettimeofday (&t1, NULL);

    while (!stable) {
      if (max_iter && iterations >= max_iter) {
        printf ("Arrêt après %d itérations\n", max_iter);
        stable = 1;
      } else {
        n = the_compute (refresh_rate);
        if (n > 0) {
          iterations += n;
          stable = 1;
          printf ("Calcul terminé en %d itérations\n", iterations);
        } else
          iterations += refresh_rate;
      }
    }

    if (opencl_used)
      ocl_wait ();

    gettimeofday (&t2, NULL);

    temps = TIME_DIFF (t1, t2);
    fprintf (stderr, "%ld.%03ld\n", temps / 1000, temps % 1000);
  }

  graphics_clean ();

  if (the_finalize != NULL)
    the_finalize ();

  return 0;
}
