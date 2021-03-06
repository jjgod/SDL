/*
 *  IMG_ImageIO.c
 *  SDL_image
 *
 *  Created by Eric Wing on 1/1/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */
#include "SDL_image.h"

// For ImageIO framework and also LaunchServices framework (for UTIs)
#include <ApplicationServices/ApplicationServices.h>

/**************************************************************
 ***** Begin Callback functions for block reading *************
 **************************************************************/

// This callback reads some bytes from an SDL_rwops and copies it
// to a Quartz buffer (supplied by Apple framework).
static size_t MyProviderGetBytesCallback(void* rwops_userdata, void* quartz_buffer, size_t the_count)
{
	return (size_t)SDL_RWread((struct SDL_RWops *)rwops_userdata, quartz_buffer, 1, the_count);
}

// This callback is triggered when the data provider is released
// so you can clean up any resources.
static void MyProviderReleaseInfoCallback(void* rwops_userdata)
{
	// What should I put here? 
	// I think the user and SDL_RWops controls closing, so I don't do anything.
}

static void MyProviderRewindCallback(void* rwops_userdata)
{
	SDL_RWseek((struct SDL_RWops *)rwops_userdata, 0, RW_SEEK_SET);
}

static void MyProviderSkipBytesCallback(void* rwops_userdata, size_t the_count)
{
	SDL_RWseek((struct SDL_RWops *)rwops_userdata, the_count, RW_SEEK_CUR);
}
/**************************************************************
 ***** End Callback functions for block reading ***************
 **************************************************************/

// This creates a CGImageSourceRef which is a handle to an image that can be used to examine information
// about the image or load the actual image data.
static CGImageSourceRef CreateCGImageSourceFromRWops(SDL_RWops* rw_ops, CFDictionaryRef hints_and_options)
{
	CGImageSourceRef source_ref;


	// Similar to SDL_RWops, Apple has their own callbacks for dealing with data streams.
	CGDataProviderCallbacks provider_callbacks =
	{
		MyProviderGetBytesCallback,
		MyProviderSkipBytesCallback,
		MyProviderRewindCallback,
		MyProviderReleaseInfoCallback
	};
	CGDataProviderRef data_provider = CGDataProviderCreate(rw_ops, &provider_callbacks);

	// Get the CGImageSourceRef.
	// The dictionary can be NULL or contain hints to help ImageIO figure out the image type.
	source_ref = CGImageSourceCreateWithDataProvider(data_provider, hints_and_options);
	return source_ref;
}


/* Create a CGImageSourceRef from a file. */
/* Remember to CFRelease the created source when done. */
static CGImageSourceRef CreateCGImageSourceFromFile(const char* the_path)
{
    CFURLRef the_url = NULL;
    CGImageSourceRef source_ref = NULL;
	CFStringRef cf_string = NULL;
	
	/* Create a CFString from a C string */
	cf_string = CFStringCreateWithCString(
										  NULL,
										  the_path,
										  kCFStringEncodingUTF8
										  );
	if(!cf_string)
	{
		return NULL;
	}
	
	/* Create a CFURL from a CFString */
    the_url = CFURLCreateWithFileSystemPath(
											NULL, 
											cf_string,
											kCFURLPOSIXPathStyle,
											false
											);
	
	/* Don't need the CFString any more (error or not) */
	CFRelease(cf_string);
	
	if(!the_url)
	{
		return NULL;
	}
	
	
    source_ref = CGImageSourceCreateWithURL(the_url, NULL);
	/* Don't need the URL any more (error or not) */
	CFRelease(the_url);
	
	return source_ref;
}



static CGImageRef CreateCGImageFromCGImageSource(CGImageSourceRef image_source)
{
	CGImageRef image_ref = NULL;
	
    if(NULL == image_source)
	{
		return NULL;
	}
	
	// Get the first item in the image source (some image formats may
	// contain multiple items).
	image_ref = CGImageSourceCreateImageAtIndex(image_source, 0, NULL);
	return image_ref;
}

static CFDictionaryRef CreateHintDictionary(CFStringRef uti_string_hint)
{
	CFDictionaryRef hint_dictionary = NULL;

	if(uti_string_hint != NULL)
	{
		// Do a bunch of work to setup a CFDictionary containing the jpeg compression properties.
		CFStringRef the_keys[1];
		CFStringRef the_values[1];
		
		the_keys[0] = kCGImageSourceTypeIdentifierHint;
		the_values[0] = uti_string_hint;
		
		// kCFTypeDictionaryKeyCallBacks or kCFCopyStringDictionaryKeyCallBacks?
		hint_dictionary = CFDictionaryCreate(NULL, (const void**)&the_keys, (const void**)&the_values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	}
	return hint_dictionary;
}




static int Internal_isType(SDL_RWops* rw_ops, CFStringRef uti_string_to_test)
{
	CGImageSourceRef image_source;
	CFStringRef uti_type;
	Boolean is_type;
	
	CFDictionaryRef hint_dictionary = NULL;
	
	hint_dictionary = CreateHintDictionary(uti_string_to_test);	
	image_source = CreateCGImageSourceFromRWops(rw_ops, hint_dictionary);
	
	if(hint_dictionary != NULL)
	{
		CFRelease(hint_dictionary);		
	}
	
	if(NULL == image_source)
	{
		return 0;
	}
	
	// This will get the UTI of the container, not the image itself.
	// Under most cases, this won't be a problem.
	// But if a person passes an icon file which contains a bmp,
	// the format will be of the icon file.
	// But I think the main SDL_image codebase has this same problem so I'm not going to worry about it.	
	uti_type = CGImageSourceGetType(image_source);
	//	CFShow(uti_type);
	
	// Unsure if we really want conformance or equality
	is_type = UTTypeConformsTo(uti_string_to_test, uti_type);
	
	CFRelease(image_source);
	
	return (int)is_type;
}

// Once we have our image, we need to get it into an SDL_Surface
static SDL_Surface* Create_SDL_Surface_From_CGImage(CGImageRef image_ref)
{
	/* This code is adapted from Apple's Documentation found here:
	 * http://developer.apple.com/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/index.html
	 * Listing 9-4††Using a Quartz image as a texture source.
	 * Unfortunately, this guide doesn't show what to do about
	 * non-RGBA image formats so I'm making the rest up.
	 * All this code should be scrutinized.
	 */
	
	size_t the_width = CGImageGetWidth(image_ref);
	size_t the_height = CGImageGetHeight(image_ref);
	CGRect the_rect = {{0, 0}, {the_width, the_height}};
	
	size_t bits_per_pixel = CGImageGetBitsPerPixel(image_ref);
	size_t bytes_per_row = CGImageGetBytesPerRow(image_ref);
	//	size_t bits_per_component = CGImageGetBitsPerComponent(image_ref);
	size_t bits_per_component = 8;
	
//	CGImageAlphaInfo alpha_info = CGImageGetAlphaInfo(image_ref);
	

	SDL_Surface* sdl_surface = NULL;
	Uint32 Rmask;
	Uint32 Gmask;
	Uint32 Bmask;
	Uint32 Amask;

	
	CGContextRef bitmap_context = NULL;
	
	CGColorSpaceRef color_space = NULL;
	CGBitmapInfo bitmap_info = CGImageGetBitmapInfo(image_ref);

	
	switch(bits_per_pixel)
	{
		case 8:
		{
			bytes_per_row = the_width*4;
			//				color_space = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
			color_space = CGColorSpaceCreateDeviceRGB();
			//				bitmap_info = kCGImageAlphaPremultipliedFirst;
#if __BIG_ENDIAN__
			bitmap_info = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Big; /* XRGB Big Endian */
#else
			bitmap_info = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little; /* XRGB Little Endian */
#endif

			Rmask = 0x00FF0000;
			Gmask = 0x0000FF00;
			Bmask = 0x000000FF;
			Amask = 0x00000000;

			sdl_surface = SDL_CreateRGBSurface(SDL_SWSURFACE,
											   the_width, the_height, 32, Rmask, Gmask, Bmask, Amask);


			
			break;
		}
		case 15:
		case 16:
		{
			bytes_per_row = the_width*4;

			color_space = CGColorSpaceCreateDeviceRGB();

#if __BIG_ENDIAN__
			bitmap_info = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Big; /* XRGB Big Endian */
#else
			bitmap_info = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little; /* XRGB Little Endian */
#endif
			Rmask = 0x00FF0000;
			Gmask = 0x0000FF00;
			Bmask = 0x000000FF;
			Amask = 0x00000000;


			sdl_surface = SDL_CreateRGBSurface(SDL_SWSURFACE,
											   the_width, the_height, 32, Rmask, Gmask, Bmask, Amask);

			break;
		}
		case 24:
		{
			bytes_per_row = the_width*4;
			//			color_space = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
			color_space = CGColorSpaceCreateDeviceRGB();
			//			bitmap_info = kCGImageAlphaNone;
#if __BIG_ENDIAN__
			bitmap_info = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Big; /* XRGB Big Endian */
#else
			bitmap_info = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little; /* XRGB Little Endian */
#endif
			Rmask = 0x00FF0000;
			Gmask = 0x0000FF00;
			Bmask = 0x000000FF;
			Amask = 0x00000000;

			sdl_surface = SDL_CreateRGBSurface(SDL_SWSURFACE,
											   the_width, the_height, 32, Rmask, Gmask, Bmask, Amask);

			break;
		}
		case 32:
		{
						
			bytes_per_row = the_width*4;
			//			color_space = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
			color_space = CGColorSpaceCreateDeviceRGB();
			//			bitmap_info = kCGImageAlphaPremultipliedFirst;
#if __BIG_ENDIAN__
			bitmap_info = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Big; /* XRGB Big Endian */
#else
			bitmap_info = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little; /* XRGB Little Endian */
#endif 

			Amask = 0xFF000000;
			Rmask = 0x00FF0000;
			Gmask = 0x0000FF00;
			Bmask = 0x000000FF;

			sdl_surface = SDL_CreateRGBSurface(SDL_SWSURFACE,
											   the_width, the_height, 32, Rmask, Gmask, Bmask, Amask);
			break;
		}
		default:
		{
            sdl_surface = NULL;
            break;
		}
			
	}

	if(NULL == sdl_surface)
	{
		if(color_space != NULL)
		{
			CGColorSpaceRelease(color_space);			
		}
		return NULL;
	}


	// Sets up a context to be drawn to with sdl_surface->pixels as the area to be drawn to
	bitmap_context = CGBitmapContextCreate(
														sdl_surface->pixels,
														the_width,
														the_height,
														bits_per_component,
														bytes_per_row,
														color_space,
														bitmap_info
														);
	
	// Draws the image into the context's image_data
	CGContextDrawImage(bitmap_context, the_rect, image_ref);
	
	CGContextRelease(bitmap_context);
	CGColorSpaceRelease(color_space);
	
	return sdl_surface;
	
	
	
}


static SDL_Surface* LoadImageFromRWops(SDL_RWops* rw_ops, CFStringRef uti_string_hint)
{
	SDL_Surface* sdl_surface;
	CGImageSourceRef image_source;
	CGImageRef image_ref = NULL;
	CFDictionaryRef hint_dictionary = NULL;

	hint_dictionary = CreateHintDictionary(uti_string_hint);
	image_source = CreateCGImageSourceFromRWops(rw_ops, hint_dictionary);

	if(hint_dictionary != NULL)
	{
		CFRelease(hint_dictionary);		
	}
	
	if(NULL == image_source)
	{
		return NULL;
	}
	
	image_ref = CreateCGImageFromCGImageSource(image_source);
	CFRelease(image_source);

	if(NULL == image_ref)
	{
		return NULL;
	}
	
	sdl_surface = Create_SDL_Surface_From_CGImage(image_ref);
	CFRelease(image_ref);
	return sdl_surface;
	
}



static SDL_Surface* LoadImageFromFile(const char* file)
{
	SDL_Surface* sdl_surface = NULL;
	CGImageSourceRef image_source = NULL;
	CGImageRef image_ref = NULL;
	
	// First ImageIO
	image_source = CreateCGImageSourceFromFile(file);
	
	if(NULL == image_source)
	{
		return NULL;
	}
	
	image_ref = CreateCGImageFromCGImageSource(image_source);
	CFRelease(image_source);
	
	if(NULL == image_ref)
	{
		return NULL;
	}
	
	sdl_surface = Create_SDL_Surface_From_CGImage(image_ref);
	CFRelease(image_ref);
	return sdl_surface;	
}


int IMG_isBMP(SDL_RWops *src)
{
	return Internal_isType(src, kUTTypeBMP);
}

int IMG_isGIF(SDL_RWops *src)
{
	return Internal_isType(src, kUTTypeGIF);
}
// Note: JPEG 2000 is kUTTypeJPEG2000
int IMG_isJPG(SDL_RWops *src)
{
	return Internal_isType(src, kUTTypeJPEG);
}

int IMG_isPNG(SDL_RWops *src)
{
	return Internal_isType(src, kUTTypePNG);
}

// This isn't a public API function. Apple seems to be able to identify tga's.
int IMG_isTGA(SDL_RWops *src)
{
	return Internal_isType(src, CFSTR("com.truevision.tga-image"));
}

int IMG_isTIF(SDL_RWops *src)
{
	return Internal_isType(src, kUTTypeTIFF);
}

SDL_Surface* IMG_LoadBMP_RW(SDL_RWops *src)
{
	return LoadImageFromRWops(src, kUTTypeBMP);
}
SDL_Surface* IMG_LoadGIF_RW(SDL_RWops *src)
{
	return LoadImageFromRWops(src, kUTTypeGIF);
}
SDL_Surface* IMG_LoadJPG_RW(SDL_RWops *src)
{
	return LoadImageFromRWops(src, kUTTypeJPEG);
}
SDL_Surface* IMG_LoadPNG_RW(SDL_RWops *src)
{
	return LoadImageFromRWops(src, kUTTypePNG);
}
SDL_Surface* IMG_LoadTGA_RW(SDL_RWops *src)
{
	return LoadImageFromRWops(src, CFSTR("com.truevision.tga-image"));
}
SDL_Surface* IMG_LoadTIF_RW(SDL_RWops *src)
{
	return LoadImageFromRWops(src, kUTTypeTIFF);
}

// Apple provides both stream and file loading functions in ImageIO.
// Potentially, Apple can optimize for either case.
SDL_Surface* IMG_Load(const char *file)
{
	SDL_Surface* sdl_surface = NULL;

	sdl_surface = LoadImageFromFile(file);
	if(NULL == sdl_surface)
	{
		// Either the file doesn't exist or ImageIO doesn't understand the format.
		// For the latter case, fallback to the native SDL_image handlers.
		SDL_RWops *src = SDL_RWFromFile(file, "rb");
		char *ext = strrchr(file, '.');
		if(ext) {
			ext++;
		}
		if(!src) {
			/* The error message has been set in SDL_RWFromFile */
			return NULL;
		}
		sdl_surface = IMG_LoadTyped_RW(src, 1, ext);
	}
	return sdl_surface;
}

