#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define BASE_ADDR 0x200
#define FONTSET_SIZE 80

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
  uint32_t screen[2048];
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
  uint8_t register_val =
      emulator_state->registers[emulator_state->opcode & 0x0F00U >> 8];
  uint8_t byte_val = emulator_state->opcode & 0x00FFU;
  if (byte_val == register_val) {
    emulator_state->program_counter += 2;
  }
  emulator_state->program_counter += 2;
}

// SNE instruction skips the next instruction if
// the value in register X is not equal to byte KK
void instr_SNE(struct emu_state *emulator_state) {
  uint8_t register_val =
      emulator_state->registers[emulator_state->opcode & 0x0F00U >> 8];
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
      emulator_state->registers[emulator_state->opcode & 0x0F00U >> 8];
  uint8_t register_y_val =
      emulator_state->registers[emulator_state->opcode & 0x00F0U >> 4];
  if (register_x_val == register_y_val) {
    emulator_state->program_counter += 2;
  }
  emulator_state->program_counter += 2;
}

// LD instruction loads the value of byte kk into register Y
void instr_LD(struct emu_state *emulator_state) {
  emulator_state->registers[emulator_state->opcode & 0x0F00U] =
      emulator_state->opcode & 0x00FFU;
  emulator_state->program_counter += 2;
}

// ADD instruction adds kk to register X
void instr_ADD(struct emu_state *emulator_state) {
  emulator_state->registers[emulator_state->opcode & 0x0F00U >> 8] +=
      emulator_state->opcode & 0x00FFU;
  emulator_state->program_counter += 2;
}

// LD instruction loads the value of register X into register Y
void instr_LD_reg(struct emu_state *emulator_state) {
  emulator_state->registers[emulator_state->opcode & 0x0F00U >> 8] =
      emulator_state->registers[emulator_state->opcode & 0x00F0U >> 4];
  emulator_state->program_counter += 2;
}

void instr_LD_I(struct emu_state *emulator_state) {
  emulator_state->index = emulator_state->opcode & 0x0FFFU;
  emulator_state->program_counter += 2;
}

void instr_AND_reg(struct emu_state *emulator_state) {
  emulator_state->registers[emulator_state->opcode & 0x0F00U >> 8] &=
      emulator_state->registers[emulator_state->opcode & 0x00F0U >> 4];
  emulator_state->program_counter += 2;
}

void instr_DRW(struct emu_state *emulator_state) {
  emulator_state->registers[emulator_state->opcode & 0x0F00U >> 8] &=
      emulator_state->registers[emulator_state->opcode & 0x00F0U >> 4];
  emulator_state->program_counter += 2;
}

void *get_op_func(uint16_t opcode) {
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

int main() {
  struct emu_state state;
  load_rom("tetris.ch8", &state);
  state.program_counter = BASE_ADDR;

  while (1 == 1) {
    state.opcode = (((uint16_t)state.memory[state.program_counter]) << 8) |
                   (uint16_t)state.memory[state.program_counter + 1];
    printf("PC: %#x / OPCODE: %#x\n", state.program_counter, state.opcode);
    void (*op_func)(struct emu_state *) = get_op_func(state.opcode);
    if (op_func == NULL) {
      printf("Illegal Instruction.\n");
      return 1;
    }
    op_func(&state);
  }

  return 0;
}
