#include "ssb.h"

#ifdef __cplusplus
extern "C" {
#endif

	const int FONTSIZE = 10; /* Main used font size */
	const char* FONTFILE = "ssbfnt.ttf";
	TTF_Font* _br_font; /* Main used font descriptor */

	int ssb_init( void )
	{
		return SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER );
	}

	void ssb_deinit( void )
	{
		SDL_Quit();
	}

	SDL_Surface* ssb_video_init( unsigned width, unsigned height, unsigned fullscreen )
	{

		const SDL_VideoInfo* info = SDL_GetVideoInfo();
		const unsigned bpp = info ->vfmt ->BitsPerPixel;
		unsigned long flags = SDL_ANYFORMAT;

		if( fullscreen )
			flags |= SDL_FULLSCREEN;

		if( info->blit_hw_CC && info->blit_fill ) {
			flags |= SDL_HWSURFACE;

			if( info ->video_mem * 1024 > height * width * bpp / 8 )
				flags |= SDL_DOUBLEBUF;
			else
				flags &= ~SDL_HWSURFACE;
		}

		return SDL_SetVideoMode( width, height, bpp, flags );
	}

	SDL_Surface* ssb_surface_create( unsigned width, unsigned height, const SDL_Surface* src )
	{
		const SDL_PixelFormat* fmt = src ->format;
		unsigned long flags = src ->flags & SDL_HWSURFACE;

		return SDL_CreateRGBSurface( flags,
		                             width,
		                             height,
		                             fmt ->BitsPerPixel,
		                             fmt ->Rmask,
		                             fmt ->Gmask,
		                             fmt ->Bmask,
		                             fmt ->Amask );
	}

	void ssb_set_src_colorkey( SDL_Surface* surface, unsigned long color )
	{
		SDL_SetColorKey( surface, SDL_SRCCOLORKEY, color );
	}

	void ssb_video_deinit( SDL_Surface* surface )
	{
		SDL_FreeSurface( surface );
	}

	void ssb_flip( SDL_Surface* surface )
	{
		SDL_Flip( surface );
	}

	/*
	 * Color is R:G:B:?
	 */
	unsigned long ssb_map_rgb_inner( SDL_Surface* surface, unsigned long RGBZ )
	{
		return RGBZ;
	}

	unsigned long ssb_map_rgb( SDL_Surface* surface, unsigned char R, unsigned char B, unsigned char G )
	{
		unsigned long color = ( R << 24 ) | ( G << 16 ) | ( B << 8 ) | 0xFF;
		return ssb_map_rgb_inner( surface, color );
	}

	void ssb_clear( SDL_Surface* surface )
	{
		SDL_FillRect( surface, 0, 0 );
	}

	void ssb_blit( SDL_Surface* dest,
	               SDL_Surface* src,
	               unsigned long src_coords,
	               unsigned long src_dimensions,
	               unsigned long dest_coords )
	{
		SDL_Rect src_r		= {0, 0, 0, 0},
		             dest_r		= {0, 0, 0, 0},
		                 *src_rp		= 0,
		                     *dest_rp	= &dest_r;

		if( src_coords || src_dimensions ) {
			src_r.x = ( short ) GETX( src_coords );
			src_r.y = ( short ) GETY( src_coords );
			src_r.h = ( unsigned short ) GETY( src_dimensions ); /* Height is Y */
			src_r.w = ( unsigned short ) GETX( src_dimensions ); /* Width is X */
			src_rp = &src_r;
		}

		dest_r.x = ( short ) GETX( dest_coords );
		dest_r.y = ( short ) GETY( dest_coords );

		SDL_BlitSurface( src, src_rp, dest, dest_rp );
	}

	SDL_Event* ssb_get_event( void )
	{
		static SDL_Event event;
		return SDL_PollEvent( &event ) ? &event
		       : 0;
	}

	/*
	 * Drawing part
	 */

	void ssb_pixel( SDL_Surface* surface,
	                unsigned long coords,
	                unsigned color )
	{
		pixelColor( surface, EXPAND( coords ), color );
	}

	void ssb_line( SDL_Surface* surface,
	               unsigned long coords_1,
	               unsigned long coords_2,
	               unsigned color )
	{
		aalineColor( surface, EXPAND( coords_1 ), EXPAND( coords_2 ), color );
	}

	void ssb_rect( SDL_Surface* surface,
	               unsigned long coords,
	               unsigned long dimensions,
	               unsigned color )
	{
		rectangleColor( surface, EXPAND( coords ), EXPAND( dimensions + coords ), color );
	}

	void ssb_box( SDL_Surface* surface,
	              unsigned long coords,
	              unsigned long dimensions,
	              unsigned color )
	{
		boxColor( surface, EXPAND( coords ), EXPAND( dimensions + coords ), color );
	}

	void ssb_circle( SDL_Surface* surface,
	                 unsigned long coords,
	                 unsigned r,
	                 unsigned color )
	{
		aacircleColor( surface, EXPAND( coords ), r, color );
	}

	void ssb_ellipse( SDL_Surface* surface,
	                  long unsigned int coords,
	                  unsigned long radius,
	                  unsigned int color )
	{
		aaellipseColor( surface, EXPAND( coords ), EXPAND( radius ), color );
	}


	void ssb_filled_circle( SDL_Surface* surface,
	                        unsigned long coords,
	                        unsigned r,
	                        unsigned color )
	{
		filledCircleColor( surface, EXPAND( coords ), r, color );
	}

	int ssb_init_text( void )
	{
		if( TTF_Init() )
			return -1; /* Failure */

		_br_font = TTF_OpenFont( FONTFILE, FONTSIZE );
		return !_br_font; /* Status depends on font pointer */
	}

	long ssb_text_extent( const char* text )
	{
		int x, y;

		TTF_SizeText( _br_font, text, &x, &y );
		return PACKWORD( y, x );
	}

	SDL_Surface* ssb_render_text( const char* text, long color )
	{
		SDL_Color fgclr = * ( SDL_Color* ) &color,
		          bgclr = {0, 0, 0, 0};

		return TTF_RenderText_Shaded( _br_font, text, fgclr, bgclr );
	}

#ifdef __cplusplus
}
#endif

// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
