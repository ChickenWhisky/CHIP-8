// SDL Libraries

// #include <SDL.h>
#include <SDL2/SDL.h>


// Standard C Libraries
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_log.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

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
	uint16_t opcpde;
	uint16_t NNN;		// 12-bit address or constant;
	uint8_t NN;			// 8-bit constant
	uint8_t N;			// 4-bit constant
	uint8_t X;			// 4-bit register identifier
	uint8_t Y;			// 4-bit register identifier
}instruction_t;

typedef struct{
	emulator_state_t state;		// State of the emulator
	uint8_t ram[4096];			// 4KB of RAM
	uint16_t stack[12];			// Stack of 12 16-bit values for upto 12 layers of nesting
	uint8_t V[16];				// 8-bit registers from V0 to VF
	uint16_t I;					// 16-bit register I
	uint8_t delay_timer;		// Allows for the delay to be accounted for for refreshing monitor
	uint8_t sound_timer;		// Allows for the sound to be accounted for for refreshing monitor	
	bool keypad[16];			// 16 keys from 0 to F (8,6,4,2 are special for directions)
	const char* rom_name;		// Pointer to the ROM which we are running
	uint16_t pc;				// Program Counter
	instruction_t inst;			// Current instruction
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
bool init_chip8(chip8_t *chip8,const char* rom_path){
	
	const uint32_t entry_point = 0x200; // Entry point for the Chip8
	
	const uint8_t font[] = {
		0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
		0x20, 0x60, 0x20, 0x20, 0x70, // 1
		0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
		0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
		0x90, 0x90, 0xF0, 0x10, 0x10, // 4
		0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
		0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
		0xF0, 0x10, 0x20, 0x40, 0x40, // 7
		0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
		0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
		0xF0, 0x90, 0xF0, 0x90, 0x90, // A
		0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
		0xF0, 0x80, 0x80, 0x80, 0xF0, // C
		0xE0, 0x90, 0x90, 0x90, 0xE0, // D
		0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
		0xF0, 0x80, 0xF0, 0x80, 0x80  // F
	};
	// Load Font 
	memcpy(&chip8->ram[0],font,sizeof(font));
	// Load ROM into Chip8
	
	FILE *rom = fopen(rom_path,"rb"); 
	if(!rom){
		SDL_Log("Could not open the ROM file %s\n",rom_path);
		// exit(EXIT_FAILURE);
		return false;
	}
	fseek(rom,0,SEEK_END);									// Get the FILE pointer to the end of the file
	const long max_size = sizeof(chip8->ram) - entry_point; // Get the maximum size of the ROM
	const long rom_size = ftell(rom);  						// ftell gives the size of the file from the current
															//  pointer to beinging of the FILE
	rewind(rom);											// Reset the FILE pointer to the beginning of the file
	
	if(rom_size>max_size){
		SDL_Log("Rom file %s is too big!! Rom size : %zu, Max size : %zu \n",rom_path,rom_size,max_size);
		return false;
	}
	
	if(fread(&chip8->ram[entry_point],rom_size,1,rom) != 1){
		SDL_Log("Could not read the ROM file %s\n",rom_path);
		return false;
	}
	
	
	
	// Set the defualt states and initialise the Chip 
	fclose(rom);
	
	chip8->state = RUNNING;   // This is the default chip8 state
	chip8->pc = 0x200;		  // This is the default program counter start in Chip8;
	chip8->rom_name = rom_path;
	return true;
}

bool set_config_from_args(config_t *config,int argc,char **argv){
	(void) argc;
	(void) argv;
	
	// Set the default values
	config->window_height = 32;   //OG Chip8 Y resolution
	config->window_width = 64;	  //OG Chip8 X resolution
	config->fg_color = 0xFFFFFFFF; // White
	config->bg_color = 0x00000000; // Black 	
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
				switch(event.key.keysym.sym){
					case SDLK_ESCAPE:		// Exits window and ends the program
						chip8->state = QUIT;
						break;
					case SDLK_SPACE:		// Pauses and unpauses the emulator
						if(chip8->state == RUNNING){
							chip8->state = PAUSED;
							puts("=====PAUSED=====");
						}else{
							chip8->state = RUNNING;
						}
						break;
					default:
						break;
				}
			case SDL_KEYUP:
				break;
			default:
				break;
		}
	}
}

void emulate_instruction(chip8_t *chip8){
	chip8->inst.opcpde = (chip8->ram[chip8->pc] << 8) | chip8->ram[chip8->pc+1];  // Each opcode is 2 bytes long
	chip8->pc +=2; // Move the program counter to the next instruction 
}

int main(int argc, char **argv){
	
	// Initialize config
	config_t config = {0} ;
	if(!set_config_from_args(&config,argc,argv))exit(EXIT_FAILURE);
	
	// Initialize SDL
	sdl_t sdl = {0};
	if(!init_sdl(&sdl,config))exit(EXIT_FAILURE);
	
	// Initialize Chip8
	chip8_t chip8 = {0};
	const char* rom_name = argv[1];
	if(!init_chip8(&chip8,rom_name))exit(EXIT_FAILURE);
	
	// Initial screen clear
	clear_screen(sdl,config);

	// Main Emulator loop
	
	while(chip8.state!=QUIT){
		
		// Handle SDL Events
		handle_inputs(&chip8);
		
		if(chip8.state == PAUSED)continue;
		
		// Emulate the instruction
		
		emulate_instruction(&chip8);
		
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