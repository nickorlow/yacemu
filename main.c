#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_quit.h>
#include <SDL2/SDL_timer.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define BASE_ADDR 0x200
#define FONTSET_SIZE 80
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32

struct emu_state {
  uint8_t registers[16];
  uint8_t memory[4096];
  uint16_t index;
  uint16_t program_counter;
  uint16_t stack[16];
  uint8_t stack_pointer;
  uint8_t sound_timer;
  uint8_t delay_timer;
  uint8_t keypad[16];
  uint32_t screen[SCREEN_WIDTH * SCREEN_HEIGHT];
  uint16_t opcode;
};

uint8_t fontset[FONTSET_SIZE] = {
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

void load_rom(char *file_name, struct emu_state *emulator_state) {
  printf("Loading ROM %s\n", file_name);

  FILE *file = fopen(file_name, "r");

  if (file == NULL) {
    printf("Error reading ROM file.");
    return;
  }

  fseek(file, 0L, SEEK_END);
  int rom_size = ftell(file);
  fseek(file, 0L, SEEK_SET);

#ifndef NDEBUG
  printf("ROM size is %d bytes\n", rom_size);
#endif

  for (int i = 0; i < rom_size; i++) {
    emulator_state->memory[BASE_ADDR + i] = (uint8_t)fgetc(file);
  }

  fclose(file);
}

// SYS is a deprecated instruction, basically a NOP
void instr_SYS(struct emu_state *emulator_state) {
  emulator_state->program_counter += 2;
}

// CLS instructions clears the screen
// we should zero out the video memory
void instr_CLS(struct emu_state *emulator_state) {
  memset(emulator_state->screen, 0, sizeof(emulator_state->screen));
  emulator_state->program_counter += 2;
}

// RET instruction returns from a function
// we should decrease stack pointer and set
// program_counter to last stack address
void instr_RET(struct emu_state *emulator_state) {
  emulator_state->stack_pointer--;
  emulator_state->program_counter =
      emulator_state->stack[emulator_state->stack_pointer];
  emulator_state->program_counter += 2;
}

// JP instruction jumps to an address without
// modifying the stack
void instr_JP(struct emu_state *emulator_state) {
  emulator_state->program_counter = emulator_state->opcode & 0x0FFFu;
}

// CALL instruction is like JP, but the stack is
// modified, kind of the reverse of RET
void instr_CALL(struct emu_state *emulator_state) {
  emulator_state->stack[emulator_state->stack_pointer] =
      emulator_state->program_counter;
  emulator_state->stack_pointer++;
  emulator_state->program_counter = emulator_state->opcode & 0x0FFFu;
}

// SE instruction skips the next instruction if
// the value in register X is equal to byte KK
void instr_SE(struct emu_state *emulator_state) {
  uint8_t reg = (emulator_state->opcode & 0x0F00U) >> 8;
  uint8_t register_val = emulator_state->registers[reg];
  uint8_t byte_val = emulator_state->opcode & 0x00FFU;

#ifndef NDEBUG
  printf("SE R[%#x] %d (reg_val: %d, skip?: %d)\n", reg, byte_val, register_val,
         register_val == byte_val);
#endif

  if (byte_val == register_val) {
    emulator_state->program_counter += 2;
  }
  emulator_state->program_counter += 2;
}

// SNE instruction skips the next instruction if
// the value in register X is not equal to byte KK
void instr_SNE(struct emu_state *emulator_state) {
  uint8_t register_val =
      emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8];
  uint8_t byte_val = emulator_state->opcode & 0x00FFU;
  if (byte_val != register_val) {
    emulator_state->program_counter += 2;
  }
  emulator_state->program_counter += 2;
}

// SE instruction skips the next instruction if
// the value in register X is equal to value in
// register Y
void instr_SE_reg(struct emu_state *emulator_state) {
  uint8_t register_x_val =
      emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8];
  uint8_t register_y_val =
      emulator_state->registers[(emulator_state->opcode & 0x00F0U) >> 4];
  if (register_x_val == register_y_val) {
    emulator_state->program_counter += 2;
  }
  emulator_state->program_counter += 2;
}

// SNE instruction skips the next instruction if
// the value in register X is equal to value in
// register Y
void instr_SNE_reg(struct emu_state *emulator_state) {
  uint8_t register_x_val =
      emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8];
  uint8_t register_y_val =
      emulator_state->registers[(emulator_state->opcode & 0x00F0U) >> 4];
  if (register_x_val != register_y_val) {
    emulator_state->program_counter += 2;
  }
  emulator_state->program_counter += 2;
}

// LD instruction loads the value of byte kk into register Y
void instr_LD(struct emu_state *emulator_state) {
  emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8] =
      emulator_state->opcode & 0x00FFU;
  emulator_state->program_counter += 2;
}

// ADD instruction adds kk to register X
void instr_ADD(struct emu_state *emulator_state) {
  uint8_t reg = (emulator_state->opcode & 0x0F00U) >> 8;
  uint8_t num = emulator_state->opcode & 0x00FFU;

#ifndef NDEBUG
  printf("ADD R[%#x] %d (was: %d, now: %d)\n", reg, num,
         emulator_state->registers[reg], emulator_state->registers[reg] + num);
#endif

  emulator_state->registers[reg] += num;
  emulator_state->program_counter += 2;
}

// LD instruction loads the value of register X into register Y
void instr_LD_reg(struct emu_state *emulator_state) {
  emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8] =
      emulator_state->registers[(emulator_state->opcode & 0x00F0U) >> 4];
  emulator_state->program_counter += 2;
}

// OR instruction ors the value of register X with register Y
void instr_OR_reg(struct emu_state *emulator_state) {
  emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8] |=
      emulator_state->registers[(emulator_state->opcode & 0x00F0U) >> 4];
  emulator_state->program_counter += 2;
}

// XOR instruction xors the value of register X with register Y
void instr_XOR_reg(struct emu_state *emulator_state) {
  emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8] ^=
      emulator_state->registers[(emulator_state->opcode & 0x00F0U) >> 4];
  emulator_state->program_counter += 2;
}

// ADD instruction adds the value of register X with register Y
void instr_ADD_reg(struct emu_state *emulator_state) {
  uint8_t prev_value =
      emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8];

  emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8] +=
      emulator_state->registers[(emulator_state->opcode & 0x00F0U) >> 4];

  emulator_state->registers[0xF] =
      (prev_value >
       emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8])
          ? 1
          : 0;

  emulator_state->program_counter += 2;
}

// SUB instruction subtracts the value of register X with register Y
void instr_SUB_reg(struct emu_state *emulator_state) {
  uint8_t prev_value =
      emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8];

  emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8] -=
      emulator_state->registers[(emulator_state->opcode & 0x00F0U) >> 4];

  // This doesn't exactly make sense, yet the tests pass....
  emulator_state->registers[0xF] =
      (prev_value <
       emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8])
          ? 0
          : 1;
  emulator_state->program_counter += 2;
}

// SHR instruction bit shifts Vx one to the right
void instr_SHR_reg(struct emu_state *emulator_state) {
  uint8_t reg_x_val =
      emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8];

  emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8] >>= 1;

  if ((reg_x_val & 0x01) == 1) {
    emulator_state->registers[0xF] = 1;
  } else {
    emulator_state->registers[0xF] = 0;
  }

  emulator_state->program_counter += 2;
}

// SHL instruction bit shifts Vx one to the left
void instr_SHL_reg(struct emu_state *emulator_state) {
  uint8_t reg_x_val =
      emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8];

  emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8] <<= 1;

  if (((reg_x_val & 0xA0) >> 7) == 1) {
    emulator_state->registers[0xF] = 1;
  } else {
    emulator_state->registers[0xF] = 0;
  }

  emulator_state->program_counter += 2;
}

// SUBN sets Vx to Vy - Vx
void instr_SUBN_reg(struct emu_state *emulator_state) {
  uint8_t reg_x_val =
      emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8];
  uint8_t reg_y_val =
      emulator_state->registers[(emulator_state->opcode & 0x00F0U) >> 4];

  emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8] =
      reg_y_val - reg_x_val;

  if (reg_y_val > reg_x_val) {
    emulator_state->registers[0xF] = 1;
  } else {
    emulator_state->registers[0xF] = 0;
  }

  emulator_state->program_counter += 2;
}

void instr_LD_I(struct emu_state *emulator_state) {
  emulator_state->index = emulator_state->opcode & 0x0FFFU;
  emulator_state->program_counter += 2;
}

void instr_LD_reg_I(struct emu_state *emulator_state) {
  unsigned int reg = (emulator_state->opcode & 0x0F00U) >> 8;
  for (unsigned int i = 0; i <= reg; i++) {
    emulator_state->registers[i] =
        emulator_state->memory[emulator_state->index + i];
  }
  emulator_state->program_counter += 2;
}

void instr_LD_I_reg(struct emu_state *emulator_state) {
  unsigned int reg = (emulator_state->opcode & 0x0F00U) >> 8;
  for (unsigned int i = 0; i <= reg; i++) {
    emulator_state->memory[emulator_state->index + i] =
        emulator_state->registers[i];
  }
  emulator_state->program_counter += 2;
}

void instr_LD_B_reg(struct emu_state *emulator_state) {
  unsigned int val =
      emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8];
  emulator_state->memory[emulator_state->index + 2] = val % 10;
  val /= 10;
  emulator_state->memory[emulator_state->index + 1] = val % 10;
  val /= 10;
  emulator_state->memory[emulator_state->index + 0] = val % 10;
  emulator_state->program_counter += 2;
}

void instr_AND_reg(struct emu_state *emulator_state) {
  emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8] &=
      emulator_state->registers[(emulator_state->opcode & 0x00F0U) >> 4];
  emulator_state->program_counter += 2;
}

void instr_ADD_I_reg(struct emu_state *emulator_state) {
  emulator_state->index +=
      emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8];
  emulator_state->program_counter += 2;
}

void instr_DRW(struct emu_state *emulator_state) {
  uint8_t x_cord =
      emulator_state->registers[(emulator_state->opcode & 0x0F00U) >> 8];
  uint8_t y_cord =
      emulator_state->registers[(emulator_state->opcode & 0x00F0U) >> 4];
  uint8_t size = emulator_state->opcode & 0x000FU;
  y_cord %= SCREEN_HEIGHT;
  x_cord %= SCREEN_WIDTH;

  emulator_state->registers[0xF] = 0;
  for (unsigned int r = 0; r < size; r++) {
    for (unsigned int c = 0; c < 8; c++) {
      uint32_t *screen_pixel =
          &emulator_state->screen[(r + y_cord) * SCREEN_WIDTH + (x_cord + c)];
      uint8_t sprite_pixel =
          emulator_state->memory[emulator_state->index + r] & (0x80U >> c);
      if (sprite_pixel) {
        if (*screen_pixel == 0xFFFFFFFF) {
          emulator_state->registers[0xF] = 1;
        }
        *screen_pixel ^= 0xFFFFFFFF;
      }
    }
  }

  emulator_state->program_counter += 2;
}

void *get_op_func(uint16_t opcode) {
  // TODO: Move to lookup table
  if (opcode == 0x00E0U) {
    return &instr_CLS;
  } else if (opcode == 0x00EEU) {
    return &instr_RET;
  } else if ((opcode & 0xF000U) == 0x0000U) {
    return NULL;
  } else if ((opcode & 0xF000U) == 0x1000U) {
    return &instr_JP;
  } else if ((opcode & 0xF000U) == 0x2000U) {
    return &instr_CALL;
  } else if ((opcode & 0xF000U) == 0x3000U) {
    return &instr_SE;
  } else if ((opcode & 0xF000U) == 0x4000U) {
    return &instr_SNE;
  } else if ((opcode & 0xF00FU) == 0x5000U) {
    return &instr_SE_reg;
  } else if ((opcode & 0xF00FU) == 0x9000U) {
    return &instr_SNE_reg;
  } else if ((opcode & 0xF00FU) == 0x8000U) {
    return &instr_LD_reg;
  } else if ((opcode & 0xF00FU) == 0x8001U) {
    return &instr_OR_reg;
  } else if ((opcode & 0xF00FU) == 0x8002U) {
    return &instr_AND_reg;
  } else if ((opcode & 0xF00FU) == 0x8003U) {
    return &instr_XOR_reg;
  } else if ((opcode & 0xF00FU) == 0x8004U) {
    return &instr_ADD_reg;
  } else if ((opcode & 0xF00FU) == 0x8005U) {
    return &instr_SUB_reg;
  } else if ((opcode & 0xF00FU) == 0x8006U) {
    return &instr_SHR_reg;
  } else if ((opcode & 0xF00FU) == 0x8007U) {
    return &instr_SUBN_reg;
  } else if ((opcode & 0xF00FU) == 0x800EU) {
    return &instr_SHL_reg;
  } else if ((opcode & 0xF0FFU) == 0xf065U) {
    return &instr_LD_reg_I;
  } else if ((opcode & 0xF0FFU) == 0xf033U) {
    return &instr_LD_B_reg;
  } else if ((opcode & 0xF0FFU) == 0xf055U) {
    return &instr_LD_I_reg;
  } else if ((opcode & 0xF0FFU) == 0xf01EU) {
    return &instr_ADD_I_reg;
  } else if ((opcode & 0xF000U) == 0xA000U) {
    return &instr_LD_I;
  } else if ((opcode & 0xF000U) == 0x6000U) {
    return &instr_LD;
  } else if ((opcode & 0xF000U) == 0x7000U) {
    return &instr_ADD;
  } else if ((opcode & 0xF000U) == 0xD000U) {
    return &instr_DRW;
  } else if ((opcode & 0xF00FU) == 0x8002U) {
    return &instr_AND_reg;
  }

  return NULL;
}

int main(int argc, char *argv[]) {
  struct emu_state state;
  load_rom(argv[1], &state);
  state.program_counter = BASE_ADDR;
  memset(state.screen, 0, sizeof(state.screen));

  SDL_Init(SDL_INIT_EVERYTHING);
  SDL_Window *window =
      SDL_CreateWindow("Chip-8 Emulator", // creates a window
                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       SCREEN_WIDTH * 10, SCREEN_HEIGHT * 10, 0);
  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

  SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                           SDL_TEXTUREACCESS_STREAMING,
                                           SCREEN_WIDTH, SCREEN_HEIGHT);

  int turbo_mode = argc == 3 && strcmp(argv[2], "turbo") == 0;

  if (turbo_mode) {
    printf("WARNING: Turbo mode has been enabled. Interpereter will run "
           "extremely fast!\n");
  }

  for (long i = 0; i != -1; i++) {
    state.opcode = (((uint16_t)state.memory[state.program_counter]) << 8) |
                   (uint16_t)state.memory[state.program_counter + 1];
    void (*op_func)(struct emu_state *) = get_op_func(state.opcode);

    printf("\n[%d] | PC: %#x / OPCODE: %#x \n", i, state.program_counter,
           state.opcode);

    if (op_func == NULL) {
      printf("Illegal Instruction.\n");
      return 1;
    }

    op_func(&state);

    SDL_UpdateTexture(texture, NULL, state.screen,
                      sizeof(state.screen[0]) * SCREEN_WIDTH);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    usleep(turbo_mode ? 100 : 16000);

    if (SDL_QuitRequested()) {
      printf("Received Quit from SDL. Goodbye!");
      break;
    }
  }

  return 0;
}
