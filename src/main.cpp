#include <SDL.h>
#include <stdio.h>
#include <android/log.h>

#define APPNAME "spc1000"

void PutPixel(int x, int y, Uint32 c, SDL_Surface *surface) {
  SDL_PixelFormat *fmt = surface->format;
  const unsigned int offset = surface->pitch * y + x * fmt->BytesPerPixel;
  unsigned char *pixels = (unsigned char *)surface->pixels;
  pixels[offset + 0] = ((c & fmt->Rmask) >> fmt->Rshift) << fmt->Rloss;
  pixels[offset + 1] = ((c & fmt->Gmask) >> fmt->Gshift) << fmt->Gloss;
  pixels[offset + 2] = ((c & fmt->Bmask) >> fmt->Bshift) << fmt->Bloss;
}

int main(int /* argc*/, char *argv[]) {
  SDL_Window *window; // Declare a pointer

  SDL_Init(SDL_INIT_VIDEO); // Initialize SDL2

  // Create an application window with the following settings:
  window = SDL_CreateWindow("An SDL2 window",        // window title
                            SDL_WINDOWPOS_UNDEFINED, // initial x position
                            SDL_WINDOWPOS_UNDEFINED, // initial y position
                            512,                     // width, in pixels
                            384,                     // height, in pixels
                            SDL_WINDOW_OPENGL        // flags - see below
  );

  // Check that the window was successfully created
  if (window == NULL) {
    // In the case that the window could not be made...
    printf("Could not create window: %s\n", SDL_GetError());
    return 1;
  }

  // The window is open: could enter program loop here (see SDL_PollEvent())
  // Setup renderer
  SDL_Renderer *renderer = NULL;
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

  SDL_Rect r;
  r.x = 0;
  r.y = 0;
  r.w = 512;
  r.h = 384;
  int width = 512;
  int height = 384;
  // Render image
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 24,
                                                        SDL_PIXELFORMAT_RGB888);
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

  // SDL_FreeSurface(surface);
  SDL_PixelFormat *fmt = surface->format;
  Uint32 c = SDL_MapRGB(surface->format, 0, 255, 0);
  int bpp = fmt->BytesPerPixel;
  unsigned char *pixels = (unsigned char *)surface->pixels;

  SDL_RendererInfo info;
  SDL_GetRendererInfo(renderer, &info);
  for (Uint32 i = 0; i < info.num_texture_formats; i++) {
    __android_log_print(ANDROID_LOG_VERBOSE, APPNAME, "format: %s",
                        SDL_GetPixelFormatName(info.texture_formats[i]));
  }
  __android_log_print(ANDROID_LOG_VERBOSE, APPNAME,
                      "(%d, %d), pitch: %d, bpp: %d", surface->w, surface->h,
                      surface->pitch, bpp);

  // splat down some random pixels
  for (int i = 0; i < 1000; i++) {
    PutPixel(rand() % 512, rand() % 384, c, surface);
  }

  SDL_UpdateTexture(texture, NULL, &pixels[0], surface->pitch);
  SDL_RenderCopy(renderer, texture, NULL, &r);
  SDL_RenderPresent(renderer);

  SDL_Delay(10 * 1000);

  // Close and destroy the window
  SDL_DestroyWindow(window);

  // Clean up
  SDL_Quit();
  return 0;
}
