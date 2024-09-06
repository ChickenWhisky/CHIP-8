// SDL Libraries

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_rect.h>

// Standard C Libraries

#include <SDL2/SDL_surface.h>
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


// All the possible states of the emulator
typedef enum{
	QUIT,
	RUNNING,
	PAUSED,
}emulator_state_t ;

// All the possible part of instruction that we could use for decoding
typedef struct{
	uint16_t opcode;
	uint16_t NNN;		// 12-bit address or constant;
	uint8_t NN;			// 8-bit constant
	uint8_t N;			// 4-bit constant
	uint8_t X;			// 4-bit register identifier
	uint8_t Y;			// 4-bit register identifier
}instruction_t;


// Chip8 Object of sorts
typedef struct{
	emulator_state_t state;		// State of the emulator
	uint8_t ram[4096];			// 4KB of RAM
	uint16_t stack[12];			// Stack of 12 16-bit values for upto 12 layers of nesting
	uint16_t* stack_ptr;				// Stack Pointer
	uint8_t V[16];				// 8-bit registers from V0 to VF
	uint16_t I;					// 16-bit register I
	uint8_t delay_timer;		// Allows for the delay to be accounted for for refreshing monitor
	uint8_t sound_timer;		// Allows for the sound to be accounted for for refreshing monitor	
	bool keypad[16];			// 16 keys from 0 to F (8,6,4,2 are special for directions)
	const char* rom_name;		// Pointer to the ROM which we are running
	uint16_t pc;				// Program Counter
	instruction_t inst;			// Current instruction
	bool display[64*32];		// display buffer
	
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

	fclose(rom);
	
	// Set the defualt states and initialise the Chip 
	chip8->state = RUNNING;   // This is the default chip8 state
	chip8->pc = 0x200;		  // This is the default program counter start in Chip8;
	chip8->rom_name = rom_path;
	chip8->stack_ptr = &chip8->stack[0];
	return true;
}

bool set_config_from_args(config_t *config,int argc,char **argv){

	// Set the default values
	config->window_height = 32;   //OG Chip8 Y resolution
	config->window_width = 64;	  //OG Chip8 X resolution
	config->fg_color = 0xFFFFFFFF; // White
	config->bg_color = 0x00000000; // Black 	
	config->scale = 20;			  // Scale of the window

	// Check if the user has provided the values
	for(int i = 2;i<argc;i++){
		if(strcmp(argv[i],"-h")==0){
			config->window_height = atoi(argv[i+1]);
		}
		if(strcmp(argv[i],"-w")==0){
			config->window_width = atoi(argv[i+1]);
		}
	}
	
	return true;
}

// For final cleanup to prevent memory leaks
void final_cleanup(sdl_t *sdl){
	SDL_DestroyRenderer(sdl->renderer); // Destroy the created renderer
	SDL_DestroyWindow(sdl->window); // Destroy the created window
	SDL_Quit(); // Shut Down SDL Subsystems
}

// Helper function to clear screen with SDL not clear screen opcode!!
void clear_screen(const sdl_t sdl,const config_t config){
	SDL_SetRenderDrawColor(sdl.renderer,
							(config.bg_color>>24)&0xFF,		// Get the red component
							(config.bg_color>>16)&0xFF,		// Get the green component
							(config.bg_color>>8)&0xFF,		// Get the blue component
							(config.bg_color>>0)&0xFF);		// Get the alpha component
	SDL_RenderClear(sdl.renderer);
}

// Helper function to draw current display buffer of Chip8 onto the screen
void update_screen(const sdl_t* sdl,const chip8_t* chip8,const config_t* config){
	// Width of each pixel is equal to that of the scale size
	SDL_Rect pixel = {.x = 0,.y = 0,.w = config->scale,.h = config->scale};
	uint8_t fg_r = (config->fg_color>>24) & 0x0FF;
	uint8_t fg_g = (config->fg_color>>16) & 0x0FF;
	uint8_t fg_b = (config->fg_color>>8 )& 0x0FF;
	uint8_t fg_a = (config->fg_color>>0 )& 0x0FF;
	
	uint8_t bg_r = (config->bg_color>>24) & 0x0FF;
	uint8_t bg_g = (config->bg_color>>16) & 0x0FF;
	uint8_t bg_b = (config->bg_color>>8 )& 0x0FF;
	uint8_t bg_a = (config->bg_color>>0 )& 0x0FF;
	
	for(uint32_t i = 0;i< sizeof(chip8->display);i++){
		pixel.x = (i%config->window_width)*config->scale;
		pixel.y = (i/config->window_width)*config->scale;
		
		if(chip8->display[i]){
			SDL_SetRenderDrawColor(sdl->renderer,fg_r,fg_g,fg_b,fg_a);
			SDL_RenderFillRect(sdl->renderer,&pixel);
		}
		else{
			SDL_SetRenderDrawColor(sdl->renderer,bg_r,bg_g,bg_b,bg_a);
			SDL_RenderFillRect(sdl->renderer,&pixel);
		}
	}
	SDL_RenderPresent(sdl->renderer);
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

#ifdef DEBUG
void print_debug_info(const chip8_t *chip8){
	printf("Address : 0x%04X , Opcode : 0x%04X , Desc :",chip8->pc-2,chip8->inst.opcode);
	switch (chip8->inst.opcode>>12 & 0x0F) {
		case(0x00):
			if(chip8->inst.NN == 0xE0){
				// Clear Screen
				// memset(&chip8->display[0],false,sizeof(chip8->display));
				printf("Clear Screen\n");
			}else if(chip8->inst.NN == 0xEE){
				// Return from subroutine
				// chip8->pc = *(--chip8->stack_ptr);
				// chip8->stack_ptr--; 
				printf("Return from subroutine to address 0x%04X\n",*(chip8->stack_ptr-1));
			}			
			break;
		case(0x01):
			// Jump to addre
			// chip8->pc = chip8->inst.NN;
			printf("Jump to address 0x%3X\n",chip8->inst.NNN);
			break;
		case(0x02):
			// Call a subroutine
			// *(chip8->stack_ptr++) = chip8->pc;  // Basically we are saving curr addres to return to once func is done
			// chip8->pc = chip8->inst.NNN; 	// Jump to the subroutine address
			printf("Call subroutine\n");
			break;
		case(0x03):
			// Skip next instruction if Vx == NN
			// if(chip8->V[chip8->inst.X] == chip8->inst.NN){
			// 	chip8->pc += 2;
			// }
			printf("Skip next instruction if V[%u] == 0x%2X",chip8->V[chip8->inst.X],chip8->inst.NN);
			break;
		case(0x04):
			// Skip next instruction if Vx != NN
			// if(chip8->V[chip8->inst.X] != chip8->inst.NN){
			// 	chip8->pc += 2;
			// }
			break;
		case(0x05):
			// Skip next instruction if Vx == Vy
			// if(chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y]){
			// 	chip8->pc += 2;
			// }
			break;
		case(0x0A):
		    // Set I to the given address NNN
			// chip8->I = chip8->inst.NNN;
			printf("Set I to 0x%04X\n",chip8->inst.NNN);
			break;
		
		case(0x06):
			// Set Vx to NN
			// chip8->V[chip8->inst.X] = chip8->inst.NN;
			printf("Set V[%d] to 0x%02X\n",chip8->inst.X,chip8->inst.NN);
			break;
		case(0x07):
			// Set Vx to Vx+=`NN
			// chip8->V[chip8->inst.X] = chip8->inst.NN;
			printf("Set V[%d] to 0x%02X\n",chip8->inst.X,chip8->V[chip8->inst.X]+chip8->inst.NN);
			break;
		case(0x08):
			// Set Vx to  Vx+=NN
			
			if(chip8->inst.N==0x0){
				// Setting Vx to Vy
				// chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
				printf("Set V[%d] to V[%d]\n",chip8->inst.X,chip8->inst.Y);
			}else if (chip8->inst.N==0x1) {
				// chip8->V[chip8->inst.X] |= chip8->V[chip8->inst.Y];
				printf("Set V[%d] to V[%d] | V[%d]\n",chip8->inst.X,chip8->inst.X,chip8->inst.Y);
			}else if (chip8->inst.N==0x2) {
				// chip8->V[chip8->inst.X] &= chip8->V[chip8->inst.Y];
				printf("Set V[%d] to V[%d] & V[%d]\n",chip8->inst.X,chip8->inst.X,chip8->inst.Y);
			}else if (chip8->inst.N==0x3) {
				// chip8->V[chip8->inst.X] ^= chip8->V[chip8->inst.Y];
				printf("Set V[%d] to V[%d] ^ V[%d]\n",chip8->inst.X,chip8->inst.X,chip8->inst.Y);
			}else if (chip8->inst.N==0x4) {
				// chip8->V[chip8->inst.X] += chip8->V[chip8->inst.Y];
				printf("Set V[%d] to V[%d] + V[%d]\n",chip8->inst.X,chip8->inst.X,chip8->inst.Y);
			}else if (chip8->inst.N==0x5) {
				// chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
				printf("Set V[%d] to V[%d] - V[%d]\n",chip8->inst.X,chip8->inst.X,chip8->inst.Y);
			}else if (chip8->inst.N==0x6) {
				// chip8->V[0xF] = chip8->V[chip8->inst.X]&1;
				// chip8->V[chip8->inst.X] >>=1;
				printf("Set V[%d] to V[%d] >> 1\n",chip8->inst.X,chip8->inst.X);
			}else if (chip8->inst.N==0x7) {
				// if(chip8->V[chip8->inst.Y] < chip8->V[chip8->inst.X])chip8->V[0xF] = 1;
				// chip8->V[chip8->inst.X] = (chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X])%256;
				printf("Set V[%d] to V[%d] - V[%d]\n",chip8->inst.X,chip8->inst.Y,chip8->inst.X);
			}else if (chip8->inst.N==0x8) {
				// chip8->V[0xF] = chip8->V[chip8->inst.X]&(1>>7);
				// chip8->V[chip8->inst.X] <<=1;
				printf("Set V[%d] to V[%d] << 1\n",chip8->inst.X,chip8->inst.X);
			}else{
				printf("Unknown Opcode\n");
			}
			
			
			break;
		case(0x0D):
			printf("Draw N (%u) height sprite at coords V%u (%02X) , V%u (%02X) from memory location I(%04X). Set VF = 1 if and pixels are turned off)\n",chip8->inst.N,chip8->inst.X,chip8->V[chip8->inst.X],chip8->inst.Y,chip8->V[chip8->inst.Y],chip8->I);
			break;
		default:
			printf("Unknown Opcode\n");
			break;		// unknown opcode or invalid
	}		
}
#endif //DEBUG

void emulate_instruction(chip8_t *chip8,const config_t* config){
	chip8->inst.opcode = (chip8->ram[chip8->pc] << 8) | chip8->ram[chip8->pc+1];  // Each opcode is 2 bytes long
	chip8->pc +=2; // Move the program counter to the next instruction 
	
	// Decode Opcodes
	// Instruction is of type DXYN each letter reps a nibble which is 4 bits long
	chip8->inst.NNN = chip8->inst.opcode & 0x0FFF;
	chip8->inst.NN = chip8->inst.opcode & 0x0FF;
	chip8->inst.N = chip8->inst.opcode & 0x0F;
	chip8->inst.X  = chip8->inst.opcode>>8 & 0x0F;
	chip8->inst.Y  = chip8->inst.opcode>>4 & 0x0F;
	
#ifdef DEBUG
	print_debug_info(chip8);
#endif //DEBUG
	
	// Emulate all opcodes
	// Look up wikipedia for the opcode implementation the switch cases are laid out according to the page
	switch (chip8->inst.opcode>>12 & 0x0F) {
		case(0x00):
			if(chip8->inst.NN == 0xE0){
				// Clear Screen
				memset(&chip8->display[0],false,sizeof(chip8->display));
			}else if(chip8->inst.NN == 0xEE){
				// Return from subroutine
				chip8->pc = *(--chip8->stack_ptr);
				chip8->stack_ptr--; 
			}
			break;
		case(0x01):
			// Jump to addre
			chip8->pc = chip8->inst.NN;
			break;
		case(0x02):
			// Call a subroutine
			*(chip8->stack_ptr++) = chip8->pc;  // Basically we are saving curr addres to return to once func is done
			chip8->pc = chip8->inst.NNN; 	// Jump to the subroutine address
			break;
		case(0x03):
			// Skip next instruction if Vx == NN
			if(chip8->V[chip8->inst.X] == chip8->inst.NN){
				chip8->pc += 2;
			}
			break;
		case(0x04):
			// Skip next instruction if Vx != NN
			if(chip8->V[chip8->inst.X] != chip8->inst.NN){
				chip8->pc += 2;
			}
			break;
		case(0x05):
			// Skip next instruction if Vx == Vy
			if(chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y]){
				chip8->pc += 2;
			}
			break;
		case(0x0A):
		    // Set I to the given address NNN
			chip8->I = chip8->inst.NNN; 
			break;
		case(0x06):
			// Set Vx to NN
			chip8->V[chip8->inst.X] = chip8->inst.NN;
			break;
		case(0x07):
			// Set Vx to  Vx+=NN
			chip8->V[chip8->inst.X] += chip8->inst.NN;
			break;
		case(0x08):
			// Set Vx to  Vx+=NN
			
			if(chip8->inst.N==0x0){
				// Setting Vx to Vy
				chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
			}else if (chip8->inst.N==0x1) {
				chip8->V[chip8->inst.X] |= chip8->V[chip8->inst.Y];
			}else if (chip8->inst.N==0x2) {
				chip8->V[chip8->inst.X] &= chip8->V[chip8->inst.Y];
			}else if (chip8->inst.N==0x3) {
				chip8->V[chip8->inst.X] ^= chip8->V[chip8->inst.Y];
			}else if (chip8->inst.N==0x4) {
				if((uint16_t)(chip8->V[chip8->inst.X]+chip8->V[chip8->inst.Y])>255)chip8->V[0xF] = 1;
				chip8->V[chip8->inst.X] += chip8->V[chip8->inst.Y];
			}else if (chip8->inst.N==0x5) {
				if(chip8->V[chip8->inst.Y] > chip8->V[chip8->inst.X])chip8->V[0xF] = 1;
				chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
			}else if (chip8->inst.N==0x6) {
				if(chip8->V[chip8->inst.X]&1)chip8->V[0xF] = 1;
				chip8->V[chip8->inst.X] >>=1;
			}else if (chip8->inst.N==0x7) {
				if(chip8->V[chip8->inst.Y] < chip8->V[chip8->inst.X])chip8->V[0xF] = 1;
				chip8->V[chip8->inst.X] = (chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X])%256;
			}else if (chip8->inst.N==0x8) {
				chip8->V[0xF] = chip8->V[chip8->inst.X]&(1>>7);
				chip8->V[chip8->inst.X] <<=1;
			}
			
			break;
		case(0x09):
			// Skip next instruction if Vx != Vy
			if(chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y]){
				chip8->pc += 2;
			}
			break;
		case(0x0B):
			chip8->pc = chip8->V[0]+chip8->inst.NNN;
			break;
		case(0x0C):
		case(0x0D):
			// Draw a sprite at position Vx, Vy with N bytes of sprite data starting at the address stored in I
			uint8_t X_coord = (chip8->V[chip8->inst.X])%config->window_width;
			uint8_t Y_coord = (chip8->V[chip8->inst.Y])%config->window_height;
			const uint8_t original_X = X_coord;	
			chip8->V[0xF] = 0; // Set the VF register to 0 initially
			
			for(int8_t i = 0;i<chip8->inst.N;i++){
				const uint8_t sprite_data = chip8->ram[chip8->I+i];
				X_coord = original_X;
				for(int8_t j = 7;j>=0;j--){
					bool *pixel = &chip8->display[Y_coord*config->window_width + X_coord];
					const bool sprite_pixel = (sprite_data & (1<<j));
					if(sprite_pixel && *pixel){
						chip8->V[0xF] = 1; // Collision detected
					}
					*pixel ^= sprite_pixel;
					
					if(++X_coord>=config->window_width)break;
				}
				if(++Y_coord>=config->window_height)break;
			}
			break;
			
		default:
			break;		// unknown opcode or invalid
	}		
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
		
		emulate_instruction(&chip8,&config);
		
		// Create a delay so that we can have it emulate 60Hz/60fps
		// 16ms is the time taken for 60Hz and we should also 
		// take into account the processing time for the op code 

		SDL_Delay(16);  

		update_screen(&sdl,&chip8,&config);
		
	}
	
	// Clean up SDL
	final_cleanup(&sdl);	
	exit(EXIT_SUCCESS);
	return 0;
}