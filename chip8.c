// SDL Libraries

// #include <SDL.h>
#include <SDL2/SDL.h>


// Standard C Libraries
#include <SDL2/SDL_events.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

// SDL Container Object
typedef struct{
	SDL_Window *window;
	SDL_Renderer *renderer;
}sdl_t ;

// Configuration for window high and width and more
typedef struct{
	uint32_t window_height;		// Height of the window
	uint32_t window_width;		// Width of the window
	uint32_t fg_color;			// Foreground color rgba888
	uint32_t bg_color;			// Background color rgba888
	uint32_t scale;				// Scale of the window
}config_t ;

typedef enum{
	QUIT,
	RUNNING,
	PAUSED,
}emulator_state_t ;

typedef struct{
	emulator_state_t state;
}chip8_t;

// Initialize SDL
bool init_sdl(sdl_t *sdl,const config_t config){
	
	
	if(SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0){
		SDL_Log("Could not initialise SDL subsytems!! %s\n",SDL_GetError());
		return false;
	}
	
	// Create a Window
	sdl->window = SDL_CreateWindow("Chip8 Emulator",
								SDL_WINDOWPOS_CENTERED,
								SDL_WINDOWPOS_CENTERED,
								config.window_width * config.scale,
							  	config.window_height * config.scale,
								SDL_WINDOW_SHOWN);
	
	// Check if window was created successfully
	if(!sdl->window){		
		SDL_Log("Could not create SDL window!! %s\n",SDL_GetError());
		return false;							
	}
	// Create the renderer
	sdl->renderer = SDL_CreateRenderer(sdl->window,-1,SDL_RENDERER_ACCELERATED);
	
	// Check if renderer was created successfully
	if(!sdl->renderer){		
		SDL_Log("Could not create SDL renderer!! %s\n",SDL_GetError());
		return false;							
	}
	
	// Sucess
	return true; 
}

// Initialize Chip8
bool init_chip8(chip8_t *chip8){
	chip8->state = RUNNING;   // This is the default chip8 state
	return true;
}

bool set_config_from_args(config_t *config,int argc,char **argv){
	(void) argc;
	(void) argv;
	
	// Set the default values
	config->window_height = 32;   //OG Chip8 Y resolution
	config->window_width = 64;	  //OG Chip8 X resolution
	config->fg_color = 0xFFFFFFFF; // White
	config->bg_color = 0xFFFF00FF; // Yellow
	config->scale = 20;			  // Scale of the window
	// Check if the user has provided the values
	for(int i = 1;i<argc;i++){
		if(strcmp(argv[i],"-h")==0){
			config->window_height = atoi(argv[i+1]);
		}
		if(strcmp(argv[i],"-w")==0){
			config->window_width = atoi(argv[i+1]);
		}
	}
	
	return true;
}

void final_cleanup(sdl_t *sdl){
	SDL_DestroyRenderer(sdl->renderer); // Destroy the created renderer
	SDL_DestroyWindow(sdl->window); // Destroy the created window
	SDL_Quit(); // Shut Down SDL Subsystems
}

void clear_screen(const sdl_t sdl,const config_t config){
	SDL_SetRenderDrawColor(sdl.renderer,
							(config.bg_color>>24)&0xFF,		// Get the red component
							(config.bg_color>>16)&0xFF,		// Get the green component
							(config.bg_color>>8)&0xFF,		// Get the blue component
							(config.bg_color>>0)&0xFF);		// Get the alpha component
	SDL_RenderClear(sdl.renderer);
}
void update_screen(const sdl_t sdl){
	SDL_RenderPresent(sdl.renderer);
}

void handle_inputs(chip8_t *chip8){
	SDL_Event event;
	while(SDL_PollEvent(&event)){
		switch(event.type){
			case SDL_QUIT:				// Exits window and ends the program
				chip8->state = QUIT;	
				exit(EXIT_SUCCESS);
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				switch(event.key.keysym.sym){
					case SDLK_ESCAPE:
						chip8->state = QUIT;
						break;
					case SDLK_SPACE:
						if(chip8->state == RUNNING){
							chip8->state = PAUSED;
						}else{
							chip8->state = RUNNING;
						}
						break;
					default:
						break;
				}
				break;
			default:
				break;
		}
	}
}

int main(int argc, char **argv){
	(void) argc;
	(void) argv;
	
	// Initialize config
	config_t config = {0} ;
	if(!set_config_from_args(&config,argc,argv))exit(EXIT_FAILURE);
	
	// Initialize SDL
	sdl_t sdl = {0};
	if(!init_sdl(&sdl,config))exit(EXIT_FAILURE);
	
	// Initialize Chip8
	chip8_t chip8 = {0};
	if(!init_chip8(&chip8))exit(EXIT_FAILURE);
	
	// Initial screen clear
	clear_screen(sdl,config);

	// Main Emulator loop
	
	while(chip8.state!=QUIT){
		
		// Handle SDL Events
		handle_inputs(&chip8);
		
		
		
		// Create a delay so that we can have it emulate 60Hz/60fps
		SDL_Delay(16);  // 16ms is the time taken for 60Hz and we should also 
												  // take into account the processing time for the op 
		update_screen(sdl);
		
	}
	
	// Clean up SDL
	final_cleanup(&sdl);
	
	
	
	exit(EXIT_SUCCESS);
	return 0;
}