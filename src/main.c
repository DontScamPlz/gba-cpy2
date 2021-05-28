/*****************************************************************************
 *
 *   SkyBoy GB Emulator
 *
 *   Copyright (c) 2021 Skyler "Sky" Saleh
 *
**/

#include "raylib.h"
#include "sb_instr_tables.h"
#include "sb_types.h"
#include <stdint.h>
#define RAYGUI_IMPLEMENTATION
#define RAYGUI_SUPPORT_ICONS
#include "raygui.h"
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#define SB_NUM_SAVE_STATES 128

#define SB_IO_JOYPAD      0xff00
#define SB_IO_SERIAL_BYTE 0xff01
#define SB_IO_DIV         0xff04
#define SB_IO_TIMA        0xff05
#define SB_IO_TMA         0xff06
#define SB_IO_TAC         0xff07
#define SB_IO_INTER_F     0xff0f
#define SB_IO_LCD_CTRL    0xff40
#define SB_IO_LCD_STAT    0xff41
#define SB_IO_LCD_SY     0xff42
#define SB_IO_LCD_SX     0xff43
#define SB_IO_LCD_LY      0xff44
#define SB_IO_LCD_LYC     0xff45
#define SB_IO_LCD_WY      0xff4A
#define SB_IO_LCD_WX      0xff4B
#define SB_IO_INTER_EN    0xffff

const int GUI_PADDING = 10;
const int GUI_ROW_HEIGHT = 30;
const int GUI_LABEL_HEIGHT = 0;
const int GUI_LABEL_PADDING = 5;

sb_emu_state_t emu_state = {.pc_breakpoint = 0};
sb_gb_t gb_state = {};

sb_gb_t sb_save_states[SB_NUM_SAVE_STATES];
int sb_valid_save_states = 0; 
unsigned sb_save_state_index=0;

void sb_pop_save_state(sb_gb_t* gb){
  if(sb_valid_save_states>0){
    --sb_valid_save_states;
    --sb_save_state_index;
    *gb = sb_save_states[sb_save_state_index%SB_NUM_SAVE_STATES];
  }
}

 
void sb_push_save_state(sb_gb_t* gb){
  ++sb_valid_save_states;
  if(sb_valid_save_states>SB_NUM_SAVE_STATES)sb_valid_save_states = SB_NUM_SAVE_STATES;
  ++sb_save_state_index;
  sb_save_states[sb_save_state_index%SB_NUM_SAVE_STATES] = *gb;
}
  
uint8_t sb_read8_direct(sb_gb_t *gb, int addr) { 
  //if(addr == 0xff80)gb->cpu.trigger_breakpoint=true;
  return gb->mem.data[addr];
}  
uint8_t sb_read8(sb_gb_t *gb, int addr) { 
  //if(addr == 0xff80)gb->cpu.trigger_breakpoint=true;
  return sb_read8_direct(gb,addr);
}
void sb_store8_direct(sb_gb_t *gb, int addr, int value) {
  static int count = 0;
  if(addr<=0x7fff){
    //printf("Attempt to write to rom address %x\n",addr);
    //gb->cpu.trigger_breakpoint=true;
    return;
  }
  if(addr == 0xdd03||addr==0xdd01){
    //printf("store: %d %x\n",count,value);
    //gb->cpu.trigger_breakpoint=true;
  }
  if(addr == SB_IO_SERIAL_BYTE){
    printf("%c",(char)value);
  }else{
    gb->mem.data[addr]=value;
  }
}
void sb_store8(sb_gb_t *gb, int addr, int value) {
  if(addr == 0xff46){
    printf("OAM Transfer\n");
    int src = value<<8;
    for(int i=0;i<=0x9F;++i){
      int d = sb_read8_direct(gb,src+i);
      sb_store8_direct(gb,0xfe00+i,d);
      
    }
  }else if(addr == SB_IO_DIV){
    value = 0; //All writes reset the div timer
  }
  sb_store8_direct(gb,addr,value);
} 
void sb_store16(sb_gb_t *gb, int addr, unsigned int value) {
  sb_store8(gb,addr,value&0xff); 
  sb_store8(gb,addr+1,((value>>8u)&0xff)); 
}
uint16_t sb_read16(sb_gb_t *gb, int addr) {
  uint16_t g = sb_read8(gb,addr+1);
  g<<=8;
  g|= sb_read8(gb,addr+0);
  return g;
}

Rectangle sb_inside_rect_after_padding(Rectangle outside_rect, int padding) {
  Rectangle rect_inside = outside_rect;
  rect_inside.x += padding;
  rect_inside.y += padding;
  rect_inside.width -= padding * 2;
  rect_inside.height -= padding * 2;
  return rect_inside;
}
void sb_vertical_adv(Rectangle outside_rect, int advance, int y_padd,
                     Rectangle *rect_top, Rectangle *rect_bottom) {
  *rect_top = outside_rect;
  rect_top->height = advance;
  *rect_bottom = outside_rect;
  rect_bottom->y += advance + y_padd;
  rect_bottom->height -= advance + y_padd;
}

Rectangle sb_draw_emu_state(Rectangle rect, sb_emu_state_t *emu_state, sb_gb_t*gb) {

  Rectangle inside_rect = sb_inside_rect_after_padding(rect, GUI_PADDING);
  Rectangle widget_rect;

  sb_vertical_adv(inside_rect, GUI_ROW_HEIGHT, GUI_PADDING, &widget_rect,
                  &inside_rect);
  widget_rect.width =
      widget_rect.width / 4 - GuiGetStyle(TOGGLE, GROUP_PADDING) * 3 / 4;
  emu_state->run_mode =
      GuiToggleGroup(widget_rect, "Reset;Pause;Run;Step", emu_state->run_mode);



                           
  sb_vertical_adv(inside_rect, GUI_ROW_HEIGHT, GUI_PADDING, &widget_rect,
                  &inside_rect);
  widget_rect.width =
      widget_rect.width / 2 - GuiGetStyle(TOGGLE, GROUP_PADDING) * 1 / 2;
      
  int save_state=GuiToggleGroup(widget_rect, "Pop Save State;Push Save State", 3);

  if(save_state ==0)sb_pop_save_state(gb);
  if(save_state ==1)sb_push_save_state(gb);
                                              
  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_LABEL_PADDING,
                  &widget_rect, &inside_rect);

  GuiLabel(widget_rect, "Instructions to Step");
  sb_vertical_adv(inside_rect, GUI_ROW_HEIGHT, GUI_PADDING, &widget_rect,
                  &inside_rect);

  static bool edit_step_instructions = false;
  if (GuiSpinner(widget_rect, "", &emu_state->step_instructions, 1, 0x7fffffff,
                 edit_step_instructions))
    edit_step_instructions = !edit_step_instructions;
  
  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_LABEL_PADDING,
                  &widget_rect, &inside_rect);

  GuiLabel(widget_rect, "Breakpoint PC");
  sb_vertical_adv(inside_rect, GUI_ROW_HEIGHT, GUI_PADDING, &widget_rect,
                  &inside_rect);

  static bool edit_bp_pc = false;
  if (GuiSpinner(widget_rect, "", &emu_state->pc_breakpoint, -1, 0x7fffffff,
                 edit_bp_pc))
    edit_bp_pc = !edit_bp_pc;
   
  Rectangle state_rect, adv_rect;
  sb_vertical_adv(rect, inside_rect.y - rect.y, GUI_PADDING, &state_rect,
                  &adv_rect);
  GuiGroupBox(state_rect, TextFormat("Emulator State [FPS: %i]", GetFPS()));
  return adv_rect;
}
Rectangle sb_draw_reg_state(Rectangle rect, const char *group_name,
                            const char **register_names, int *values) {
  Rectangle inside_rect = sb_inside_rect_after_padding(rect, GUI_PADDING);
  Rectangle widget_rect;
  while (*register_names) {
    sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING + 5,
                    &widget_rect, &inside_rect);
    GuiLabel(widget_rect, *register_names);
    int w = (inside_rect.width - GUI_PADDING * 2) / 3;
    widget_rect.x += w;
    GuiLabel(widget_rect, TextFormat("0x%X", *values));

    widget_rect.x += w + GUI_PADDING * 2;
    GuiLabel(widget_rect, TextFormat("%i", *values));

    ++register_names;
    ++values;
  }

  Rectangle state_rect, adv_rect;
  sb_vertical_adv(rect, inside_rect.y - rect.y, GUI_PADDING, &state_rect,
                  &adv_rect);
  return adv_rect;
}

Rectangle sb_draw_flag_state(Rectangle rect, const char *group_name,
                             const char **register_names, bool *values) {
  Rectangle inside_rect = sb_inside_rect_after_padding(rect, GUI_PADDING);
  Rectangle widget_rect;
  while (*register_names) {
    sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING + 5,
                    &widget_rect, &inside_rect);
    widget_rect.width = GUI_PADDING;
    widget_rect.height = GUI_PADDING;

    GuiCheckBox(
        widget_rect,
        TextFormat("%s (%s)", *register_names, (*values) ? "true" : "false"),
        *values);
    ++register_names;
    ++values;
  }

  Rectangle state_rect, adv_rect;
  sb_vertical_adv(rect, inside_rect.y - rect.y, GUI_PADDING, &state_rect,
                  &adv_rect);
  return adv_rect;
}
Rectangle sb_draw_instructions(Rectangle rect, sb_gb_cpu_t *cpu_state,
                               sb_gb_t *gb) {
  Rectangle inside_rect = sb_inside_rect_after_padding(rect, GUI_PADDING);
  Rectangle widget_rect;
  for (int i = -6; i < 5; ++i) {
    sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING + 5,
                    &widget_rect, &inside_rect);
    int pc_render = i + cpu_state->pc;

    if (pc_render < 0) {
      widget_rect.x += 80;

      GuiLabel(widget_rect, "INVALID");
    } else {
      if (i == 0)
        GuiLabel(widget_rect, "PC->");
      widget_rect.x += 30;
      GuiLabel(widget_rect, TextFormat("%06d", pc_render));
      widget_rect.x += 50;
      int opcode = sb_read8(gb, pc_render);
      GuiLabel(widget_rect, sb_decode_table[opcode].opcode_name);
      ;
      widget_rect.x += 100;
      GuiLabel(widget_rect, TextFormat("(%02x)", sb_read8(gb, pc_render)));
      widget_rect.x += 50;
    }
  }
  Rectangle state_rect, adv_rect;
  sb_vertical_adv(rect, inside_rect.y - rect.y, GUI_PADDING, &state_rect,
                  &adv_rect);
  GuiGroupBox(state_rect, "Instructions");
  return adv_rect;
}
 
Rectangle sb_draw_joypad_state(Rectangle rect, sb_gb_joy_t *joy) {

  Rectangle inside_rect = sb_inside_rect_after_padding(rect, GUI_PADDING);
  Rectangle widget_rect;
  Rectangle wr = widget_rect;
  wr.width = GUI_PADDING;
  wr.height = GUI_PADDING;
                                                                
  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING, &widget_rect,  &inside_rect);
  wr.y=widget_rect.y;
  GuiCheckBox(wr,"Up",joy->up);
  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING, &widget_rect,  &inside_rect);
  wr.y=widget_rect.y;
  GuiCheckBox(wr,"Down",joy->down);
  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING, &widget_rect,  &inside_rect);
  wr.y=widget_rect.y;
  GuiCheckBox(wr,"Left",joy->left);
  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING, &widget_rect,  &inside_rect);
  wr.y=widget_rect.y;
  GuiCheckBox(wr,"Right",joy->right);
                                                                
  inside_rect = sb_inside_rect_after_padding(rect, GUI_PADDING);
  inside_rect.x +=rect.width/2;
  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING, &widget_rect,  &inside_rect);
  wr.x +=rect.width/2;
  wr.y=widget_rect.y;
  GuiCheckBox(wr,"A",joy->a);
  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING, &widget_rect,  &inside_rect);
  wr.y=widget_rect.y;
  GuiCheckBox(wr,"B",joy->b);
  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING, &widget_rect,  &inside_rect);
  wr.y=widget_rect.y;
  GuiCheckBox(wr,"Start",joy->start);
  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING, &widget_rect,  &inside_rect);
  wr.y=widget_rect.y;
  GuiCheckBox(wr,"Select",joy->select);
  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING, &widget_rect,  &inside_rect);
                          
  Rectangle state_rect, adv_rect;
  sb_vertical_adv(rect, inside_rect.y - rect.y, GUI_PADDING, &state_rect,
                  &adv_rect);
  GuiGroupBox(state_rect, "Joypad State");
  return adv_rect;
}          
Rectangle sb_draw_timer_state(Rectangle rect, sb_gb_t *gb) {

  Rectangle inside_rect = sb_inside_rect_after_padding(rect, GUI_PADDING);
  Rectangle widget_rect;
  Rectangle wr = widget_rect;
  wr.width = GUI_PADDING;
  wr.height = GUI_PADDING;
                                                                

  int div = sb_read8_direct(gb, SB_IO_DIV);
  int tima = sb_read8_direct(gb, SB_IO_TIMA);
  int tma = sb_read8_direct(gb, SB_IO_TMA);
  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING, &widget_rect,  &inside_rect);
  GuiLabel(widget_rect, TextFormat("DIV: %d", div));
  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING, &widget_rect,  &inside_rect);
  GuiLabel(widget_rect, TextFormat("TIMA: %d", tima));
  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING, &widget_rect,  &inside_rect);
  GuiLabel(widget_rect, TextFormat("TMA: %d", tma));

                                                             
  inside_rect = sb_inside_rect_after_padding(rect, GUI_PADDING);
  inside_rect.x +=rect.width/2;
  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING, &widget_rect,  &inside_rect);
  GuiLabel(widget_rect, TextFormat("CLKs to DIV: %d", gb->timers.clocks_till_div_inc));
  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING, &widget_rect,  &inside_rect);
  GuiLabel(widget_rect, TextFormat("CLKs to TIMA: %d", gb->timers.clocks_till_tima_inc));
  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING, &widget_rect,  &inside_rect);
                              
  Rectangle state_rect, adv_rect;
  sb_vertical_adv(rect, inside_rect.y - rect.y, GUI_PADDING, &state_rect,
                  &adv_rect); 
  GuiGroupBox(state_rect, "Timer State");
  return adv_rect;
}
Rectangle sb_draw_cartridge_state(Rectangle rect,
                                  sb_gb_cartridge_t *cart_state) {

  Rectangle inside_rect = sb_inside_rect_after_padding(rect, GUI_PADDING);
  Rectangle widget_rect;

  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING, &widget_rect,
                  &inside_rect);
  GuiLabel(widget_rect, TextFormat("Title: %s", cart_state->title));

  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING + 10, &widget_rect,
                  &inside_rect);

  Rectangle wr = widget_rect;wr.width = wr.height = GUI_PADDING;

  GuiCheckBox(wr,
              TextFormat("Game Boy Color (%s)",
                         (cart_state->game_boy_color) ? "true" : "false"),
              cart_state->game_boy_color);

  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING + 5, &widget_rect,
                  &inside_rect);
  GuiLabel(widget_rect, TextFormat("Cart Type: %d", cart_state->type));

  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING + 5, &widget_rect,
                  &inside_rect);
  GuiLabel(widget_rect, TextFormat("ROM Size: %d", cart_state->rom_size));

  sb_vertical_adv(inside_rect, GUI_LABEL_HEIGHT, GUI_PADDING + 5, &widget_rect,
                  &inside_rect);
  GuiLabel(widget_rect, TextFormat("RAM Size: %d", cart_state->ram_size));

  Rectangle state_rect, adv_rect;
  sb_vertical_adv(rect, inside_rect.y - rect.y, GUI_PADDING, &state_rect,
                  &adv_rect);
  GuiGroupBox(state_rect, "Cartridge State (Drag and Drop .GBC to Load ROM)");
  return adv_rect;
}
Rectangle sb_draw_tile_state(Rectangle rect, sb_gb_cpu_t *cpu_state,
                            sb_gb_t *gb) {

  Rectangle inside_rect = sb_inside_rect_after_padding(rect, GUI_PADDING);
  Rectangle widget_rect;

  const char *register_names_16b[] = {"AF", "BC", "DE", "HL", "SP", "PC", NULL};

  int register_values_16b[] = {cpu_state->af, cpu_state->bc, cpu_state->de,
                               cpu_state->hl, cpu_state->sp, cpu_state->pc};

  const char *register_names_8b[] = {"A", "F", "B", "C", "D",
                                     "E", "H", "L", NULL};

  int register_values_8b[] = {
      SB_U16_HI(cpu_state->af), SB_U16_LO(cpu_state->af),
      SB_U16_HI(cpu_state->bc), SB_U16_LO(cpu_state->bc),
      SB_U16_HI(cpu_state->de), SB_U16_LO(cpu_state->de),
      SB_U16_HI(cpu_state->hl), SB_U16_LO(cpu_state->hl),
  };

  const char *flag_names[] = {"Z", "N", "H", "C","Inter. En.", "Prefix",  NULL};

  bool flag_values[] = {
      SB_BFE(cpu_state->af, SB_Z_BIT, 1), // Z
      SB_BFE(cpu_state->af, SB_N_BIT, 1), // N
      SB_BFE(cpu_state->af, SB_H_BIT, 1), // H
      SB_BFE(cpu_state->af, SB_C_BIT, 1), // C
      cpu_state->interrupt_enable, 
      cpu_state->prefix_op
  };
  // Split registers into three rects horizontally
  {
    Rectangle in_rect[3];
    const char *sections[] = {"16-bit Registers", "8-bit Registers", "Flags"};
    int orig_y = inside_rect.y;
    int x_off = 0;
    for (int i = 0; i < 3; ++i) {
      in_rect[i] = inside_rect;
      in_rect[i].width = inside_rect.width / 3 - GUI_PADDING * 2 / 3;
      in_rect[i].x += x_off;
      x_off += in_rect[i].width + GUI_PADDING;
    }
    in_rect[0] = sb_draw_reg_state(in_rect[0], "16-bit Registers",
                                   register_names_16b, register_values_16b);

    in_rect[1] = sb_draw_reg_state(in_rect[1], "8-bit Registers",
                                   register_names_8b, register_values_8b);

    in_rect[2] =
        sb_draw_flag_state(in_rect[2], "Flags", flag_names, flag_values);
    for (int i = 0; i < 3; ++i) {
      if (inside_rect.y < in_rect[i].y)
        inside_rect.y = in_rect[i].y;
    }
    for (int i = 0; i < 3; ++i) {
      in_rect[i].height = inside_rect.y - orig_y - GUI_PADDING;
      in_rect[i].y = orig_y;
      GuiGroupBox(in_rect[i], sections[i]);
    }

    inside_rect.height -= inside_rect.y - orig_y;
  }

  inside_rect = sb_draw_instructions(inside_rect, cpu_state, gb);

  Rectangle state_rect, adv_rect;
  sb_vertical_adv(rect, inside_rect.y - rect.y, GUI_PADDING, &state_rect,
                  &adv_rect);
  GuiGroupBox(state_rect, "Tile Data");
  return adv_rect;
}
Rectangle sb_draw_cpu_state(Rectangle rect, sb_gb_cpu_t *cpu_state,
                            sb_gb_t *gb) {

  Rectangle inside_rect = sb_inside_rect_after_padding(rect, GUI_PADDING);
  Rectangle widget_rect;

  const char *register_names_16b[] = {"AF", "BC", "DE", "HL", "SP", "PC", NULL};

  int register_values_16b[] = {cpu_state->af, cpu_state->bc, cpu_state->de,
                               cpu_state->hl, cpu_state->sp, cpu_state->pc};

  const char *register_names_8b[] = {"A", "F", "B", "C", "D",
                                     "E", "H", "L", NULL};

  int register_values_8b[] = {
      SB_U16_HI(cpu_state->af), SB_U16_LO(cpu_state->af),
      SB_U16_HI(cpu_state->bc), SB_U16_LO(cpu_state->bc),
      SB_U16_HI(cpu_state->de), SB_U16_LO(cpu_state->de),
      SB_U16_HI(cpu_state->hl), SB_U16_LO(cpu_state->hl),
  };

  const char *flag_names[] = {"Z", "N", "H", "C","Inter. En.", "Prefix",  NULL};

  bool flag_values[] = {
      SB_BFE(cpu_state->af, SB_Z_BIT, 1), // Z
      SB_BFE(cpu_state->af, SB_N_BIT, 1), // N
      SB_BFE(cpu_state->af, SB_H_BIT, 1), // H
      SB_BFE(cpu_state->af, SB_C_BIT, 1), // C
      cpu_state->interrupt_enable, 
      cpu_state->prefix_op
  };
  // Split registers into three rects horizontally
  {
    Rectangle in_rect[3];
    const char *sections[] = {"16-bit Registers", "8-bit Registers", "Flags"};
    int orig_y = inside_rect.y;
    int x_off = 0;
    for (int i = 0; i < 3; ++i) {
      in_rect[i] = inside_rect;
      in_rect[i].width = inside_rect.width / 3 - GUI_PADDING * 2 / 3;
      in_rect[i].x += x_off;
      x_off += in_rect[i].width + GUI_PADDING;
    }
    in_rect[0] = sb_draw_reg_state(in_rect[0], "16-bit Registers",
                                   register_names_16b, register_values_16b);

    in_rect[1] = sb_draw_reg_state(in_rect[1], "8-bit Registers",
                                   register_names_8b, register_values_8b);

    in_rect[2] =
        sb_draw_flag_state(in_rect[2], "Flags", flag_names, flag_values);
    for (int i = 0; i < 3; ++i) {
      if (inside_rect.y < in_rect[i].y)
        inside_rect.y = in_rect[i].y;
    }
    for (int i = 0; i < 3; ++i) {
      in_rect[i].height = inside_rect.y - orig_y - GUI_PADDING;
      in_rect[i].y = orig_y;
      GuiGroupBox(in_rect[i], sections[i]);
    }

    inside_rect.height -= inside_rect.y - orig_y;
  }

  inside_rect = sb_draw_instructions(inside_rect, cpu_state, gb);

  Rectangle state_rect, adv_rect;
  sb_vertical_adv(rect, inside_rect.y - rect.y, GUI_PADDING, &state_rect,
                  &adv_rect);
  GuiGroupBox(state_rect, "CPU State");
  return adv_rect;
}
void sb_update_joypad_io_reg(sb_emu_state_t* state, sb_gb_t*gb){
  // FF00 - P1/JOYP - Joypad (R/W)
  //
  // Bit 7 - Not used
  // Bit 6 - Not used
  // Bit 5 - P15 Select Action buttons    (0=Select)
  // Bit 4 - P14 Select Direction buttons (0=Select)
  // Bit 3 - P13 Input: Down  or Start    (0=Pressed) (Read Only)
  // Bit 2 - P12 Input: Up    or Select   (0=Pressed) (Read Only)
  // Bit 1 - P11 Input: Left  or B        (0=Pressed) (Read Only)
  // Bit 0 - P10 Input: Right or A        (0=Pressed) (Read Only)

  uint8_t data_dir =    ((!gb->joy.down)<<3)| ((!gb->joy.up)<<2)    |((!gb->joy.left)<<1)|((!gb->joy.right));  
  uint8_t data_action = ((!gb->joy.start)<<3)|((!gb->joy.select)<<2)|((!gb->joy.b)<<1)   |(!gb->joy.a);

  uint8_t data = gb->mem.data[SB_IO_JOYPAD];
  
  data&=0xf0;
  
  if(0 == (data & (1<<4))) data |= data_dir;
  if(0 == (data & (1<<5))) data |= data_action;

  gb->mem.data[SB_IO_JOYPAD] = data;

}        
void sb_poll_controller_input(sb_gb_t* gb){

  gb->joy.left  = IsKeyDown(KEY_A);
  gb->joy.right = IsKeyDown(KEY_D);
  gb->joy.up    = IsKeyDown(KEY_W);
  gb->joy.down  = IsKeyDown(KEY_S);
  gb->joy.a = IsKeyDown(KEY_J);
  gb->joy.b = IsKeyDown(KEY_K);
  gb->joy.start = IsKeyDown(KEY_ENTER);
  gb->joy.select = IsKeyDown(KEY_APOSTROPHE);

}

bool sb_update_lcd_status(sb_gb_t* gb, int delta_cycles){
  uint8_t stat = sb_read8_direct(gb, SB_IO_LCD_STAT); 
  uint8_t ctrl = sb_read8_direct(gb, SB_IO_LCD_CTRL);
  uint8_t ly  = sb_read8_direct(gb, SB_IO_LCD_LY);
  uint8_t lyc = sb_read8_direct(gb, SB_IO_LCD_LYC);
  bool enable = SB_BFE(ctrl,7,1)==1;
  int mode = 0; 
  if(!enable){
    gb->lcd.scanline_cycles = 0;
    ly = 0;   
  }


  const int mode2_clks= 80;
  // TODO: mode 3 is 10 cycles longer for every sprite intersected
  const int mode3_clks = 168;
  const int mode0_clks = 208;
  const int scanline_dots = 456; 

  bool new_scanline = false; 
  gb->lcd.scanline_cycles +=delta_cycles;
  if(gb->lcd.scanline_cycles>=scanline_dots){
    gb->lcd.scanline_cycles-=scanline_dots;
    new_scanline = true;
    ly+=1; 
  }

  if(ly > 153) ly = 0; 

  if(gb->lcd.scanline_cycles<=mode2_clks)mode = 2;
  else if(gb->lcd.scanline_cycles<=mode3_clks+mode2_clks) mode =3;
  else mode =0;

  if(ly>=154) {ly=0;}
  if(ly>=144) {mode = 1; new_scanline = false;} 
  
  if(ly ==SB_LCD_H){
    uint8_t inter_flag = sb_read8_direct(gb, SB_IO_INTER_F);
    //V-BLANK Interrupt
    sb_store8_direct(gb, SB_IO_INTER_F, inter_flag| (1<<0));
  }
  if(ly == lyc){
    uint8_t inter_flag = sb_read8_direct(gb, SB_IO_INTER_F);
    //LCD-stat Interrupt
    sb_store8_direct(gb, SB_IO_INTER_F, inter_flag| (1<<1));
    mode|=0x4;
  }
  stat = (stat&0xf8) | mode; 
  sb_store8_direct(gb, SB_IO_LCD_STAT, stat);
  sb_store8_direct(gb, SB_IO_LCD_LY, ly);
  return new_scanline; 
}
void sb_draw_scanline(sb_gb_t*gb){
  uint8_t ctrl = sb_read8_direct(gb, SB_IO_LCD_CTRL);
  bool draw_bg     = SB_BFE(ctrl,0,1)==1;
  bool draw_sprite = SB_BFE(ctrl,1,1)==1;
  bool sprite8x16  = SB_BFE(ctrl,2,1)==1;
  int bg_tile_map_base      = SB_BFE(ctrl,3,1)==1 ? 0x9c00 : 0x9800;  
  int bg_win_tile_data_mode = SB_BFE(ctrl,4,1)==1;  
  bool window_enable = SB_BFE(ctrl,5,1)==1;  
  int win_tile_map_base      = SB_BFE(ctrl,6,1)==1 ? 0x9c00 : 0x9800;  
  
  int oam_table_offset = 0xfe00;
  uint8_t y = sb_read8_direct(gb, SB_IO_LCD_LY);
  int wx = sb_read8_direct(gb, SB_IO_LCD_WX);
  int wy = sb_read8_direct(gb, SB_IO_LCD_WY);
  int sx = sb_read8_direct(gb, SB_IO_LCD_SX);
  int sy = sb_read8_direct(gb, SB_IO_LCD_SY);

  int sprite_h = sprite8x16 ? 16: 8;
  const int sprites_per_scanline = 10;
  // HW only draws first 10 sprites that touch a scanline
  int render_sprites[40];
  int sprite_index=0; 
  
  for(int i=0;i<sprites_per_scanline;++i)render_sprites[i]=-1;
  const int num_sprites= 40;     
  for(int i=0;i<num_sprites;++i){
    int sprite_base = oam_table_offset+i*4;
    int yc = sb_read8_direct(gb, sprite_base+0)-16;
    int xc = sb_read8_direct(gb, sprite_base+1)-16; 
    if(yc<=y && yc+sprite_h>y&& sprite_index<sprites_per_scanline){
      render_sprites[sprite_index++]=i;
    }
  }

  for(int x = 0; x < SB_LCD_W; ++x){
      
    uint8_t r=0,g=0,b=0;

    if(draw_bg){
      int px = x+ sx; 
      int py = y+ sy;
      const int tile_size = 8;
      const int tiles_per_row = 32;
      int tile_offset = ((px/tile_size)+(py/tile_size)*tiles_per_row)&0x3ff;
      
      int tile_id = sb_read8_direct(gb, bg_tile_map_base+tile_offset);

      int pixel_in_tile_x = 7-(px%8);
      int pixel_in_tile_y = (py%8);

      int bytes_per_tile = 2*8;

      int byte_tile_data_off = 0;

      if(bg_win_tile_data_mode==0){
        byte_tile_data_off = 0x8000 + 0x1000 + (((int8_t)(tile_id))*bytes_per_tile);
      }else{
        byte_tile_data_off = 0x8000 + (((uint8_t)(tile_id))*bytes_per_tile);
      }
      byte_tile_data_off+=pixel_in_tile_y*2;
      uint8_t data1 = sb_read8_direct(gb, byte_tile_data_off);
      uint8_t data2 = sb_read8_direct(gb, byte_tile_data_off+1);
      uint8_t color_id = 3-(SB_BFE(data2,pixel_in_tile_x,1)+SB_BFE(data1,pixel_in_tile_x,1)*2);


      for(int i=0;i<sprites_per_scanline;++i){
        int sprite = render_sprites[i];
        if(sprite==-1)continue;
        int sprite_base = oam_table_offset+sprite*4;
        int yc = sb_read8_direct(gb, sprite_base+0)-16;
        int xc = sb_read8_direct(gb, sprite_base+1)-8;
        
        int tile = sb_read8_direct(gb, sprite_base+2);
        int attr = sb_read8_direct(gb, sprite_base+3);

        int x_sprite = 7-(x-xc); 
        int y_sprite = y-yc;
        //Check if the sprite is hit
        if(x_sprite>=8 || x_sprite<0)continue;
        if(y_sprite<0 || y_sprite>=16 || (y_sprite>=8 && sprite8x16==false))continue;

        int byte_tile_data_off = 0x8000 + (((uint8_t)(tile))*bytes_per_tile);
        byte_tile_data_off+=y_sprite*2;

        uint8_t data1 = sb_read8_direct(gb, byte_tile_data_off);
        uint8_t data2 = sb_read8_direct(gb, byte_tile_data_off+1);

        color_id = 3-(SB_BFE(data2,x_sprite,1)+SB_BFE(data1,x_sprite,1)*2);
        
      }
      r = color_id*85;
      g = color_id*85;
      b = color_id*85;
      //r = SB_BFE(tile_id,0,3)*31;
      //g = SB_BFE(tile_id,3,3)*31;
      //b = SB_BFE(tile_id,6,2)*63;

    }
     

    gb->lcd.framebuffer[(x+(y)*SB_LCD_W)*3+0] = r;     
    gb->lcd.framebuffer[(x+(y)*SB_LCD_W)*3+1] = g;     
    gb->lcd.framebuffer[(x+(y)*SB_LCD_W)*3+2] = b;     
  }
}
void sb_update_lcd(sb_gb_t* gb, int delta_cycles){
  bool new_scanline = sb_update_lcd_status(gb, delta_cycles);
  if(new_scanline){
    sb_draw_scanline(gb);
  }
}
void sb_update_timers(sb_gb_t* gb, int delta_clocks){
  uint8_t tac = sb_read8_direct(gb, SB_IO_TAC);
  bool tima_enable = SB_BFE(tac, 2, 1);
  int clk_sel = SB_BFE(tac, 0, 2);
  gb->timers.clocks_till_div_inc -=delta_clocks;
  if(gb->timers.clocks_till_div_inc<0){
    int period = 4*1024*1024/16384; 
    gb->timers.clocks_till_div_inc+=period;
    if(gb->timers.clocks_till_div_inc<0)
      gb->timers.clocks_till_div_inc = period; 

    uint8_t d = sb_read8_direct(gb, SB_IO_DIV);
    sb_store8_direct(gb, SB_IO_DIV, d+1); 
  }
  gb->timers.clocks_till_tima_inc -=delta_clocks;
  if(gb->timers.clocks_till_tima_inc<0){
    int period =0;
    switch(clk_sel){
      case 0: period = 1024; break; 
      case 1: period = 16; break; 
      case 2: period = 64; break; 
      case 3: period = 256; break; 
    }
    gb->timers.clocks_till_tima_inc+=period;
    if(gb->timers.clocks_till_tima_inc<0)
      gb->timers.clocks_till_tima_inc = period; 

    uint8_t d = sb_read8_direct(gb, SB_IO_TIMA);
    // Trigger timer interrupt 
    if(d == 255){
      uint8_t i_flag = sb_read8_direct(gb, SB_IO_INTER_F);
      i_flag |= 1<<2;
      sb_store8_direct(gb, SB_IO_INTER_F, i_flag); 
      
    }
    sb_store8_direct(gb, SB_IO_TIMA, d+1); 
  }  
}
void sb_tick(){
  static FILE* file = NULL;
  sb_poll_controller_input(&gb_state);
  if (emu_state.run_mode == SB_MODE_RESET) {
    if(file)fclose(file);
    file = fopen("instr_trace.txt","wb");
    memset(&gb_state.cpu, 0, sizeof(gb_state.cpu));
    
    gb_state.cpu.pc = 0x100;

    gb_state.cpu.af=0x01B0;
    gb_state.cpu.bc=0x0013;
    gb_state.cpu.de=0x00D8;
    gb_state.cpu.hl=0x014D;
    gb_state.cpu.sp=0xFFFE;

    gb_state.mem.data[0xFF05] = 0x00; // TIMA
    gb_state.mem.data[0xFF06] = 0x00; // TMA
    gb_state.mem.data[0xFF07] = 0x00; // TAC
    gb_state.mem.data[0xFF10] = 0x80; // NR10
    gb_state.mem.data[0xFF11] = 0xBF; // NR11
    gb_state.mem.data[0xFF12] = 0xF3; // NR12
    gb_state.mem.data[0xFF14] = 0xBF; // NR14
    gb_state.mem.data[0xFF16] = 0x3F; // NR21
    gb_state.mem.data[0xFF17] = 0x00; // NR22
    gb_state.mem.data[0xFF19] = 0xBF; // NR24
    gb_state.mem.data[0xFF1A] = 0x7F; // NR30
    gb_state.mem.data[0xFF1B] = 0xFF; // NR31
    gb_state.mem.data[0xFF1C] = 0x9F; // NR32
    gb_state.mem.data[0xFF1E] = 0xBF; // NR34
    gb_state.mem.data[0xFF20] = 0xFF; // NR41
    gb_state.mem.data[0xFF21] = 0x00; // NR42
    gb_state.mem.data[0xFF22] = 0x00; // NR43
    gb_state.mem.data[0xFF23] = 0xBF; // NR44
    gb_state.mem.data[0xFF24] = 0x77; // NR50
    gb_state.mem.data[0xFF25] = 0xF3; // NR51
    gb_state.mem.data[0xFF26] = 0xF1; // $F0-SGB ; NR52
    gb_state.mem.data[0xFF40] = 0x91; // LCDC
    gb_state.mem.data[0xFF42] = 0x00; // SCY
    gb_state.mem.data[0xFF43] = 0x00; // SCX
    gb_state.mem.data[0xFF44] = 0x90; // SCX
    gb_state.mem.data[0xFF45] = 0x00; // LYC
    gb_state.mem.data[0xFF47] = 0xFC; // BGP
    gb_state.mem.data[0xFF48] = 0xFF; // OBP0
    gb_state.mem.data[0xFF49] = 0xFF; // OBP1
    gb_state.mem.data[0xFF4A] = 0x00; // WY
    gb_state.mem.data[0xFF4B] = 0x00; // WX
    gb_state.mem.data[0xFFFF] = 0x00; // IE
    
    gb_state.timers.clocks_till_div_inc=0;
    gb_state.timers.clocks_till_tima_inc=0;

    for(int i=0;i<SB_LCD_W*SB_LCD_H*3;++i){
      gb_state.lcd.framebuffer[i] = GetRandomValue(0,255);
    }
  }
  
  if (emu_state.run_mode == SB_MODE_RUN||emu_state.run_mode ==SB_MODE_STEP) {
    
    int instructions_to_execute = emu_state.step_instructions;
    for(int i=0;i<instructions_to_execute;++i){
    
        sb_update_joypad_io_reg(&emu_state, &gb_state);
        int pc = gb_state.cpu.pc;
        

        unsigned op = sb_read8(&gb_state,gb_state.cpu.pc);

        //if(gb_state.cpu.pc == 0xC65F)gb_state.cpu.trigger_breakpoint =true;
        if(gb_state.cpu.prefix_op==false)
        fprintf(file,"A: %02X F: %02X B: %02X C: %02X D: %02X E: %02X H: %02X L: %02X SP: %04X PC: 00:%04X (%02X %02X %02X %02X)\n",
          SB_U16_HI(gb_state.cpu.af),SB_U16_LO(gb_state.cpu.af),
          SB_U16_HI(gb_state.cpu.bc),SB_U16_LO(gb_state.cpu.bc),
          SB_U16_HI(gb_state.cpu.de),SB_U16_LO(gb_state.cpu.de),
          SB_U16_HI(gb_state.cpu.hl),SB_U16_LO(gb_state.cpu.hl),
          gb_state.cpu.sp,pc,
          sb_read8(&gb_state,pc),
          sb_read8(&gb_state,pc+1),
          sb_read8(&gb_state,pc+2),
          sb_read8(&gb_state,pc+3)
          );      
          
        if(gb_state.cpu.prefix_op)op+=256;
           
        int trigger_interrupt = -1;
        // TODO: Can interrupts trigger between prefix ops and the second byte?
        if(gb_state.cpu.interrupt_enable && gb_state.cpu.prefix_op==false){
          uint8_t ie = sb_read8_direct(&gb_state,SB_IO_INTER_EN);
          uint8_t i_flag = sb_read8_direct(&gb_state,SB_IO_INTER_F);
          uint8_t masked_interupt = ie&i_flag&0x1f;

          for(int i=0;i<5;++i){
            if(masked_interupt & (1<<i)){trigger_interrupt = i;break;}
          }
          if(trigger_interrupt!=-1)i_flag &= ~(1<<trigger_interrupt);
          //if(trigger_interrupt!=-1)gb_state.cpu.trigger_breakpoint = true; 
          sb_store8_direct(&gb_state,SB_IO_INTER_F,i_flag);
        }

        gb_state.cpu.prefix_op = false;
        int delta_cycles = 0;
        if(trigger_interrupt!=-1){
          int interrupt_address = (trigger_interrupt*0x8)+0x40;
          sb_call_impl(&gb_state, interrupt_address, 0, 0, 0, (const uint8_t*)"----");
          delta_cycles = 5*4;
        }else{
          sb_instr_t inst = sb_decode_table[op];
          gb_state.cpu.pc+=inst.length;
          int operand1 = sb_load_operand(&gb_state,inst.op_src1);
          int operand2 = sb_load_operand(&gb_state,inst.op_src2);
                                  
          int pc_before_inst = gb_state.cpu.pc; 
          inst.impl(&gb_state, operand1, operand2,inst.op_src1,inst.op_src2, inst.flag_mask);
          if(gb_state.cpu.prefix_op==true)i--;

          delta_cycles = 4*(gb_state.cpu.pc==pc_before_inst? inst.mcycles : inst.mcycles_branch_taken);
        }
        sb_update_lcd(&gb_state,delta_cycles);
        sb_update_timers(&gb_state,delta_cycles);
                                
        //sb_push_save_state(&gb_state);

        if (gb_state.cpu.pc == emu_state.pc_breakpoint||gb_state.cpu.trigger_breakpoint){
          gb_state.cpu.trigger_breakpoint = false; 
          emu_state.run_mode = SB_MODE_PAUSE;
          break;                   
        }                            
        
    }
  }
 
  if (emu_state.run_mode == SB_MODE_STEP) {
    emu_state.run_mode = SB_MODE_PAUSE;
  }
}
void sb_draw_sidebar(Rectangle rect) {
  GuiPanel(rect);
  Rectangle rect_inside = sb_inside_rect_after_padding(rect, GUI_PADDING);

  rect_inside = sb_draw_emu_state(rect_inside, &emu_state,&gb_state);
  rect_inside = sb_draw_cartridge_state(rect_inside, &gb_state.cart);
  rect_inside = sb_draw_timer_state(rect_inside, &gb_state);
  rect_inside = sb_draw_joypad_state(rect_inside, &gb_state.joy);
  rect_inside = sb_draw_cpu_state(rect_inside, &gb_state.cpu, &gb_state);
                               
}

bool showContentArea = true;
void UpdateDrawFrame() {
  if (IsFileDropped()) {
    int count = 0;
    char **files = GetDroppedFiles(&count);
    if (count > 0) {
      unsigned int bytes = 0;
      unsigned char *data = LoadFileData(files[0], &bytes);
      printf("Dropped File: %s, %d bytes\n", files[0], bytes);

      for (size_t i = 0; i < bytes; ++i) {
        gb_state.mem.data[i] = gb_state.cart.data[i] = data[i];
      }
      // Copy Header
      for (int i = 0; i < 11; ++i) {
        gb_state.cart.title[i] = gb_state.cart.data[i + 0x134];
      }
      // TODO PGB Mode(Values with Bit 7 set, and either Bit 2 or 3 set)
      gb_state.cart.game_boy_color =
          SB_BFE(gb_state.cart.data[0x143], 7, 1) == 1;
      gb_state.cart.type = gb_state.cart.data[0x147];

      switch (gb_state.cart.data[0x148]) {
      case 0x0:
        gb_state.cart.rom_size = 32 * 1024;
        break;
      case 0x1:
        gb_state.cart.rom_size = 64 * 1024;
        break;
      case 0x2:
        gb_state.cart.rom_size = 128 * 1024;
        break;
      case 0x3:
        gb_state.cart.rom_size = 256 * 1024;
        break;
      case 0x4:
        gb_state.cart.rom_size = 512 * 1024;
        break;
      case 0x5:
        gb_state.cart.rom_size = 1024 * 1024;
        break;
      case 0x6:
        gb_state.cart.rom_size = 2 * 1024 * 1024;
        break;
      case 0x7:
        gb_state.cart.rom_size = 4 * 1024 * 1024;
        break;
      case 0x8:
        gb_state.cart.rom_size = 8 * 1024 * 1024;
        break;
      case 0x52:
        gb_state.cart.rom_size = 1.1 * 1024 * 1024;
        break;
      case 0x53:
        gb_state.cart.rom_size = 1.2 * 1024 * 1024;
        break;
      case 0x54:
        gb_state.cart.rom_size = 1.5 * 1024 * 1024;
        break;
      default:
        gb_state.cart.rom_size = 32 * 1024;
        break;
      }

      switch (gb_state.cart.data[0x149]) {
      case 0x0:
        gb_state.cart.ram_size = 0;
        break;
      case 0x1:
        gb_state.cart.ram_size = 0;
        break;
      case 0x2:
        gb_state.cart.ram_size = 8 * 1024;
        break;
      case 0x3:
        gb_state.cart.ram_size = 32 * 1024;
        break;
      case 0x4:
        gb_state.cart.ram_size = 128 * 1024;
        break;
      case 0x5:
        gb_state.cart.ram_size = 64 * 1024;
        break;
      default:
        break;
      }
      emu_state.run_mode = SB_MODE_RESET;

      UnloadFileData(data);
    }
    ClearDroppedFiles();
  }
  sb_tick();

  // Draw
  //-----------------------------------------------------
  BeginDrawing();

  ClearBackground(RAYWHITE);
  sb_draw_sidebar((Rectangle){0, 0, 400, GetScreenHeight()});
                  
 
  Image screenIm = {
        .data = gb_state.lcd.framebuffer,
        .width = SB_LCD_W,
        .height = SB_LCD_H,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8,
        .mipmaps = 1
  };
    
  Texture2D screenTex = LoadTextureFromImage(screenIm); 
  SetTextureFilter(screenTex, TEXTURE_FILTER_POINT);
  Rectangle rect;
  rect.x = 400;
  rect.y = 0;
  rect.width = GetScreenWidth()-400;
  rect.height = GetScreenHeight();

  DrawTextureQuad(screenTex, (Vector2){1.f,1.f}, (Vector2){0.0f,0.0},rect, (Color){255,255,255,255}); 
   
  EndDrawing();
  UnloadTexture(screenTex);
}

int main(void) {
  // Initialization
  //---------------------------------------------------------
  const int screenWidth = 1200;
  const int screenHeight = 700;

  // Set configuration flags for window creation
  SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
  InitWindow(screenWidth, screenHeight, "SkyBoy");
  SetTraceLogLevel(LOG_WARNING);
#if defined(PLATFORM_WEB)
  emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
  SetTargetFPS(60);

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    UpdateDrawFrame();
  }
#endif

  CloseWindow(); // Close window and OpenGL context

  return 0;
}
