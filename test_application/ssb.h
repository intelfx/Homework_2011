#ifndef SDLBRIDGE_H_
#define SDLBRIDGE_H_

#include <SDL/SDL.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_ttf.h>

#define GETBYTE(packed, index) (((packed) >> ((index) * 8 )) & 0xFF  )
#define GETWORD(packed, index) (((packed) >> ((index) * 16)) & 0xFFFF)

/* All coords and dimensions are packed DWORDs as following: (Y[WORD]:X[WORD]),
 * except surface ones.
 */

#define GETX(coords) GETWORD(coords, 0)
#define GETY(coords) GETWORD(coords, 1)
#define EXPAND(coords) GETX(coords),GETY(coords)
#define PACKWORD(_y, _x) ((((_y) & 0xFFFF) << 16) | ((_x) & 0xFFFF))


#define RGB()


#ifdef __cplusplus
extern "C" {
#endif

	/* Library control functions */
	int ssb_init( void );
	void ssb_deinit( void );

	/* Video subsystem functions */
	SDL_Surface* ssb_video_init( unsigned width, unsigned height, unsigned fullscreen );
	void ssb_video_deinit( SDL_Surface* surface );

	SDL_Surface* ssb_surface_create( unsigned width, unsigned height, const SDL_Surface* src );

	void ssb_set_src_colorkey( SDL_Surface* surface, unsigned long color );

	void ssb_flip( SDL_Surface* surface );
	void ssb_blit( SDL_Surface* dest,
	               SDL_Surface* src,
	               unsigned long src_coords,
	               unsigned long src_dimensions,
	               unsigned long dest_coords ); /* All coords are packed Y:X */

	void ssb_clear( SDL_Surface* surface );

	unsigned long ssb_map_rgb_inner( SDL_Surface* surface, unsigned long RGBZ ); /* Color is R:G:B:? */
	unsigned long ssb_map_rgb( SDL_Surface* surface, unsigned char R, unsigned char B, unsigned char G );
	/* unsigned long MapRGBA (SDL_Surface* surface, unsigned long RGBZ, unsigned alpha); */

	/* Event subsystem functions */
	/* Event subsystem is minimalistic */
	SDL_Event* ssb_get_event( void );

	/* Drawing (SDL_draw) */
	void ssb_pixel( SDL_Surface* surface,
	                unsigned long coords,
	                unsigned color );

	void ssb_line( SDL_Surface* surface,
	               unsigned long coords_1,
	               unsigned long coords_2,
	               unsigned color );

	void ssb_rect( SDL_Surface* surface,
	               unsigned long coords,
	               unsigned long dimensions,
	               unsigned color );

	void ssb_box( SDL_Surface* surface,
	              unsigned long coords,
	              unsigned long dimensions,
	              unsigned color );

	void ssb_circle( SDL_Surface* surface,
	                 unsigned long coords,
	                 unsigned r,
	                 unsigned color );

	void ssb_ellipse( SDL_Surface* surface,
	                  unsigned long coords,
	                  unsigned long radius,
	                  unsigned color );

	void ssb_filled_circle( SDL_Surface* surface,
	                        unsigned long coords,
	                        unsigned r,
	                        unsigned color );

	/* Text (SDL_ttf) */
	extern const int FONTSIZE; /* Main used font size */
	extern const char* FONTFILE;
	extern TTF_Font* _br_font; /* Main used font descriptor */

	int ssb_init_text( void );
	long ssb_text_extent( const char* text ); /* Returns packed Y:X dimensions of the rendered text*/
	SDL_Surface* ssb_render_text( const char* text, long color ); /* Renders text to a new surface */

#ifdef __cplusplus
}
#endif

#endif /* SDLBRIDGE_H_ */
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4; 
