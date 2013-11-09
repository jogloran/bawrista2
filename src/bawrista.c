#include <pebble.h>
#include <stdlib.h>

#define TIMER_COOKIE 24601
#define GRECT_OFFSET(r, dx, dy) GRect((r).origin.x+(dx), (r).origin.y+(dy), (r).size.w, (r).size.h)
#define MY_UUID { 0x53, 0x0D, 0x98, 0xCE, 0x47, 0x8D, 0x4D, 0x17, 0xA8, 0x6F, 0xC6, 0xA2, 0x5A, 0x54, 0xB9, 0xBC }
PBL_APP_INFO(MY_UUID,
             "baWrista", "Overpunch",
             1, 0, /* App version */
             DEFAULT_MENU_ICON,
             APP_INFO_STANDARD_APP);

Window* window;
TextLayer* steep_text_layer;
TextLayer* press_text_layer;
TextLayer* instr_text_layer;
Layer image_layer;

typedef enum { SET_STEEP, SET_PRESS, STEEP, PRESS, DONE } AppState;
AppState state;

char steep_time_buffer[12 + 5] = "Steep time: 0";
char press_time_buffer[12 + 5] = "Press time: 0";
char steep_msg[] = "Set time to Steep";
char press_msg[] = "Set time to Press";
char enjoy_msg[] = "Enjoy!";

int steep_interval; // interval*15 seconds
int press_interval; // interval*15 seconds
int steep_increment = 15;
int press_increment = 1;

RotBmpPairContainer base_bitmap;
// RotBmpPairContainer plunger_bitmap;
// BmpContainer base_bitmap;
BmpContainer plunger_bitmap;
PropertyAnimation prop_animation;

// Yet, another good itoa implementation
void itoa(int value, char *sp, int radix)
{
    char tmp[16];// be careful with the length of the buffer
    char *tp = tmp;
    int i;
    unsigned v;
    int sign;

    sign = (radix == 10 && value < 0);
    if (sign)   v = -value;
    else    v = (unsigned)value;

    while (v || tp == tmp)
    {
        i = v % radix;
        v /= radix; // v/=radix uses less CPU clocks than v=v/radix does
        if (i < 10)
          *tp++ = i+'0';
        else
          *tp++ = i + 'a' - 10;
    }

    if (sign)
    *sp++ = '-';
    while (tp > tmp)
    *sp++ = *--tp;

    *sp++ = '\0';
}

void update(void) {
    itoa(steep_interval*steep_increment, steep_time_buffer+12, 10);
    text_layer_set_text(steep_text_layer, steep_time_buffer);

    itoa(press_interval*press_increment, press_time_buffer+12, 10);
    text_layer_set_text(press_text_layer, press_time_buffer);
    
    char* msg;
    switch (state) {
      case SET_STEEP:
        msg = steep_msg;
        break;
      case SET_PRESS:
        msg = press_msg;
        break;
      case STEEP:
        msg = steep_msg + 12;
        break;
      case PRESS:
        msg = press_msg + 12;
        break;
      case DONE:
      default:
        msg = enjoy_msg;
        break;
    }
    switch (state) {
      case SET_STEEP:
      case SET_PRESS:
      text_layer_set_background_color(instr_text_layer, GColorBlack);
      text_layer_set_text_color(instr_text_layer, GColorWhite);
      break;
      case STEEP: case PRESS: case DONE:
      text_layer_set_background_color(instr_text_layer, GColorWhite);
      text_layer_set_text_color(instr_text_layer, GColorBlack);
      break;
    }
    text_layer_set_text(instr_text_layer, msg);
}

void reset_times_and_update() {
  // defaults: steep 60 seconds, press 10 seconds
  steep_increment = 15;
  press_increment = 1;
  
  steep_interval = 4;
  press_interval = 10;
  update();
}


void up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  int* var = NULL;
  if (state == SET_STEEP) {
      var = &steep_interval;
  } else if (state == SET_PRESS) {
      var = &press_interval;
  }

  if (var != NULL) {
      if (*var > 1) {
        --*var;
        update();
      }
  }
}


void down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  int* var = NULL;
  if (state == SET_STEEP) {
      var = &steep_interval;
  } else if (state == SET_PRESS) {
      var = &press_interval;
  }

  if (var != NULL) {
      if (*var < 20) {
        ++*var;
        update();
      }
  }
}

void start_aeropress_timer() {
  steep_increment = 15;
  press_increment = 1;
  
  app_timer_send_event(app_ctx, steep_increment*1000, TIMER_COOKIE);
}

void select_click_handler(ClickRecognizerRef recognizer, Window *window) {
  switch (state) {
    case SET_STEEP:
      ++state;
      update();
      break;
    case SET_PRESS:
      ++state;
      update();
      start_aeropress_timer();
      break;
    case STEEP: case PRESS:
      break;
    case DONE:
      state = SET_STEEP;

      layer_set_frame(&base_bitmap.layer.layer,    GRect(18, 10, 100, 141));
      layer_set_frame(&plunger_bitmap.layer.layer, GRECT_OFFSET(layer_get_frame(&image_layer), 0, -36));

      reset_times_and_update();
      break;
  }
}


void click_config_provider(ClickConfig **config, Window *window) {
  (void)window;

  config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_single_click_handler;
  config[BUTTON_ID_UP]->click.repeat_interval_ms = 10000;
  
  config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) select_click_handler;
  config[BUTTON_ID_SELECT]->click.repeat_interval_ms = 10000;

  config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_single_click_handler;
  config[BUTTON_ID_DOWN]->click.repeat_interval_ms = 10000;
}

void handle_plunger_animation_done(struct Animation *animation, bool finished, void *context) {
}


void handle_timer(AppContextRef app_ctx, AppTimerHandle handle, uint32_t cookie) {
  if (state == STEEP) {
    --steep_interval;
    
    if (steep_interval == 0) {
      ++state;
      vibes_short_pulse();

      animation_set_duration(&prop_animation.animation, press_interval*press_increment*1000);
      animation_schedule(&prop_animation.animation);
      
      app_timer_send_event(app_ctx, press_increment*1000, TIMER_COOKIE);
    } else {
      app_timer_send_event(app_ctx, steep_increment*1000, TIMER_COOKIE);
    }
  } else if (state == PRESS) {
    --press_interval;
    
    if (press_interval == 0) {
      ++state;
      vibes_short_pulse();
    } else {
      app_timer_send_event(app_ctx, press_increment*1000, TIMER_COOKIE);
    }
  }
  
  update();
}

void handle_init() {
  state = SET_STEEP;

  window = window_create();
  window_stack_push(window, true);
  
  resource_init_current_app(&FONT_DEMO_RESOURCES);

  steep_text_layer = text_layer_create(GRect(0, 0, 144, 15));
  text_layer_set_text_alignment(steep_text_layer, GTextAlignmentCenter);
  text_layer_set_text(steep_text_layer, steep_time_buffer);
  text_layer_set_font(steep_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  layer_add_child(&window.layer, steep_text_layer.layer);
  
  press_text_layer = text_layer_create(GRect(0, 15, 144, 15));
  text_layer_set_text_alignment(press_text_layer, GTextAlignmentCenter);
  text_layer_set_text(press_text_layer, press_time_buffer);
  text_layer_set_font(press_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  layer_add_child(&window.layer, press_text_layer.layer);
  
  instr_text_layer = text_layer_create(GRect(0, 36, 144, 22));
  text_layer_set_text_alignment(instr_text_layer, GTextAlignmentCenter);
  text_layer_set_text(instr_text_layer, steep_msg);
  text_layer_set_font(instr_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(&window.layer, instr_text_layer.layer);

  reset_times_and_update();
  
  layer_init(&image_layer, GRECT_OFFSET(window.layer.frame, 0, 36));
  
  bmp_init_container(RESOURCE_ID_IMAGE_PLUNGER, &plunger_bitmap);
  rotbmp_pair_init_container(RESOURCE_ID_IMAGE_BASE_WHITE, 
                             RESOURCE_ID_IMAGE_BASE_BLACK, &base_bitmap);

  layer_set_frame(&base_bitmap.layer.layer,    GRect(18, 10, 100, 141));
  layer_set_frame(&plunger_bitmap.layer.layer, GRECT_OFFSET(layer_get_frame(&image_layer), 0, -36));
                        
  layer_add_child(&image_layer, &plunger_bitmap.layer.layer);
  layer_add_child(&image_layer, &base_bitmap.layer.layer);
  
  layer_add_child(&window.layer, &image_layer);
  
  GRect from_rect = GRECT_OFFSET(layer_get_frame(&image_layer), 0, -36);
  GRect to_rect = GRect(from_rect.origin.x, from_rect.origin.y+20, from_rect.size.w, from_rect.size.h);
  property_animation_init_layer_frame(&prop_animation, &plunger_bitmap.layer.layer, &from_rect, &to_rect);
  
  AnimationHandlers animation_handlers = {
    .stopped = &handle_plunger_animation_done
  };
  
  animation_set_handlers(&prop_animation.animation, animation_handlers, NULL);
  
  
  window_set_click_config_provider(&window, (ClickConfigProvider) click_config_provider);
}

void handle_deinit() {
  window_destroy(window);

  // rotbmp_pair_deinit_container(&plunger_bitmap);
  rotbmp_pair_deinit_container(&base_bitmap);
  bmp_deinit_container(&plunger_bitmap);
  //bmp_deinit_container(&base_bitmap);
}

int main(void) {
  // PebbleAppHandlers handlers = {
  //   .init_handler = &handle_init,
  //   .deinit_handler = &handle_deinit,
  //   .timer_handler = &handle_timer
  // };
  // app_event_loop(params, &handlers);
  handle_init();
  app_event_loop();
  handle_deinit();
}
