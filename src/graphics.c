
#include <SDL_image.h>
#include <SDL_opengl.h>

#include "constants.h"
#include "global.h"
#include "graphics.h"
#include "compute.h"
#include "draw.h"
#include "error.h"
#include "debug.h"
#include "ocl.h"

static SDL_Window *win = NULL;
static SDL_Renderer *ren = NULL;
static SDL_Surface *surface = NULL;
static SDL_Texture *texture = NULL;
//static SDL_Texture *alt_texture = NULL;

char *pngfile = NULL;

unsigned display = 1;
unsigned vsync = 1;
unsigned do_first_touch = 0;
char *draw_param = NULL;

Uint32 * restrict image = NULL, * restrict alt_image = NULL;
unsigned DIM = 0;



static void graphics_create_surface (unsigned dim)
{
  Uint32 rmask, gmask, bmask, amask;

  rmask = 0xff000000;
  gmask = 0x00ff0000;
  bmask = 0x0000ff00;
  amask = 0x000000ff;

  DIM = dim;
  image = malloc (dim * dim * sizeof (Uint32));
  alt_image = malloc (dim * dim * sizeof (Uint32));

  if (do_first_touch) {
    if (the_first_touch != NULL) {
      printf ("Using first touch allocation policy\n");
      the_first_touch ();
    } else
      printf ("*** Sorry, no first touch policy found for current version ***\n");
  }

   if (pngfile == NULL  && !display)
   return;
  surface = SDL_CreateRGBSurfaceFrom (image, dim, dim, 32,
                                      dim * sizeof (Uint32),
                                      rmask, gmask, bmask, amask);
  if (surface == NULL)
    exit_with_error ("SDL_CreateRGBSurfaceFrom () failed: %s", SDL_GetError ());
}

static void graphics_load_surface (char *filename)
{
  SDL_Surface *old;
  unsigned size;

  // Chargement de l'image
  old = IMG_Load (filename);
  if (old == NULL)
    exit_with_error ("IMG_Load: <%s>\n", filename);

  size = MIN (old->w, old->h);
  if (DIM)
    size = MIN (DIM, size);
  
  graphics_create_surface (size);

  // copie de old vers surface
  {
    SDL_Rect src;

    src.x = 0;
    src.y = 0;
    src.w = size;
    src.h = size;

    SDL_BlitSurface (old, /* src */
		     &src,
		     surface, /* dest */
		     NULL);
  }

  SDL_FreeSurface (old);
}

void graphics_image_init (void)
{
  // Nettoyage de la transparence
  for (int i = 0; i < DIM; i++)
    for (int j = 0; j < DIM; j++)
      if ((cur_img (i, j) & 0xFF) == 0)
	// Si la composante alpha est nulle, on met l'ensemble du pixel à zéro
	cur_img (i, j) = 0;
      else
	cur_img (i, j) |= 0xFF;

  // Appel de la fonction de dessin spécifique, si elle existe
  if (the_draw != NULL)
    the_draw (draw_param ? : "");
}

void graphics_init ()
{
  Uint32 render_flags = SDL_RENDERER_ACCELERATED | (vsync ? SDL_RENDERER_PRESENTVSYNC : 0);

  // Initialisation de SDL
  if (pngfile != NULL  || display)
    if (SDL_Init (SDL_INIT_VIDEO) != 0)
      exit_with_error ("SDL_Init");

  if (display) {

    // Création de la fenêtre sur l'écran
    win = SDL_CreateWindow ("Hello World!", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			    WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_SHOWN);
    if (win == NULL)
      exit_with_error ("SDL_CreateWindow");

    // Initialisation du moteur de rendu
    ren = SDL_CreateRenderer (win, -1, render_flags);
    if (ren == NULL)
      exit_with_error ("SDL_CreateRenderer");
  }

  if (pngfile == NULL) {
    unsigned size = DIM ? DIM : DEFAULT_DIM;

    // Note: First touch is performed inside graphics_create_surface
    graphics_create_surface (size);
    
    memset (image, 0, DIM * DIM * sizeof (Uint32));
  } else
    graphics_load_surface (pngfile);

  graphics_image_init ();

  memcpy (alt_image, image, DIM * DIM * sizeof (Uint32));

  // Création d'une texture à partir de la surface
  //texture = SDL_CreateTextureFromSurface (ren, surface);
  texture = SDL_CreateTexture (ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
			       DIM, DIM);
  PRINT_DEBUG ('g', "DIM = %d\n", DIM);

#ifdef __APPLE__
  // FIXME: Ugly fix for SDL2 bug under Mojave
  {SDL_Event evt; SDL_PollEvent (&evt);}
#endif
}

void graphics_share_texture_buffers (void)
{
  GLuint texid;

  SDL_GL_BindTexture (texture, NULL, NULL);

  glGetIntegerv (GL_TEXTURE_BINDING_2D, (GLint *)&texid);

  ocl_map_textures (texid);
}

void graphics_render_image (void)
{
  SDL_Rect src, dst;

  // Refresh texture
  if (opencl_used) {
    
    glFinish ();
    ocl_update_texture ();

  } else {
    SDL_GL_BindTexture (texture, NULL, NULL);

    glTexSubImage2D (GL_TEXTURE_2D,
		     0, /* mipmap level */
		     0, 0, /* x, y */
		     DIM, DIM, /* width, height */
		     GL_RGBA,
		     GL_UNSIGNED_INT_8_8_8_8,
		     image);
  }
  
  src.x = 0;
  src.y = 0;
  src.w = DIM;
  src.h = DIM;

  // On redimensionne l'image pour qu'elle occupe toute la fenêtre
  dst.x = 0;
  dst.y = 0;
  dst.w = WIN_WIDTH;
  dst.h = WIN_HEIGHT;

  SDL_RenderCopy (ren, texture, &src, &dst);
}

void graphics_refresh (void)
{
  // On efface la scène dans le moteur de rendu (inutile !)
  SDL_RenderClear (ren);

  // On réaffiche l'image
  graphics_render_image ();
  
  // Met à jour l'affichage sur écran
  SDL_RenderPresent (ren);
}

void graphics_clean (void)
{
  if (display) {
    
    if (ren != NULL)
      SDL_DestroyRenderer (ren);
    else
      return;

    if (win != NULL)
      SDL_DestroyWindow (win);
    else
      return;
  }

  if (image != NULL)
    free (image);

  if (surface != NULL)
      SDL_FreeSurface (surface);

  if (texture != NULL)
    SDL_DestroyTexture (texture);

  IMG_Quit ();
  SDL_Quit ();
}

int graphics_display_enabled (void)
{
  return display;
}
