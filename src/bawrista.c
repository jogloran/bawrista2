#include <pebble.h>
#include <stdlib.h>

#define TIMER_COOKIE 24601
#define GRECT_OFFSET(r, dx, dy) GRect((r).origin.x+(dx), (r).origin.y+(dy), (r).size.w, (r).size.h)
#define BASE_BITMAP_REST_POSITION (GRect(22, 0, 100, 141))

Window* window;
TextLayer* steep_text_layer;
TextLayer* press_text_layer;
TextLayer* instr_text_layer;
Layer* image_layer;

GBitmap* base_white_resource;
GBitmap* base_black_resource;
GBitmap* plunger_resource;

AppTimer* timer;

typedef enum { SET_STEEP, SET_PRESS, STEEP, WAIT, PRESS, DONE } AppState;
AppState state;

uint32_t steep_time_key = 0xc4f3c4fe;
uint32_t press_time_key = 0xc4f3c4ff;

char steep_time_buffer[12 + 5] = "Steep time: 0";
char press_time_buffer[12 + 5] = "Press time: 0";
char steep_msg[] = "Set time to Steep";
char press_msg[] = "Set time to Press";
char wait_msg[] = "Get ready to Press";
char enjoy_msg[] = "Enjoy!";

int steep_interval;
int press_interval;
#define DEFAULT_WAIT_INTERVAL 2
int wait_interval = DEFAULT_WAIT_INTERVAL;

int steep_increment;
int press_increment;
#define STEEP_INCREMENT 5
#define PRESS_INCREMENT 1
int wait_increment = 1;

BitmapLayer* base_bitmap_white;
BitmapLayer* base_bitmap_black;

BitmapLayer* plunger_bitmap;
PropertyAnimation* prop_animation;

#define DEFAULT_STEEP_TIME (60 / (STEEP_INCREMENT))
#define DEFAULT_PRESS_TIME (10 / (PRESS_INCREMENT))
typedef enum { STEEP_INTERVAL_TYPE, PRESS_INTERVAL_TYPE } IntervalType;

int get_interval(IntervalType type) {
  int result = 0;
  int default_value = 0;
  uint32_t key = 0;

  switch (type) {
    case STEEP_INTERVAL_TYPE:
    key = steep_time_key;
    default_value = DEFAULT_STEEP_TIME;
    break;

    case PRESS_INTERVAL_TYPE:
    key = press_time_key; 
    default_value = DEFAULT_PRESS_TIME;
    break;
  }

  result = persist_read_int(key);
  if (result == 0) {
    persist_write_int(key, default_value);
    result = default_value;
  }

  return result;
}

void set_interval(IntervalType type, int value) {
  uint32_t key = 0;

  switch (type) {
    case STEEP_INTERVAL_TYPE:
    key = steep_time_key;
    break;

    case PRESS_INTERVAL_TYPE:
    key = press_time_key; 
    break;
  }

  persist_write_int(key, value);
}

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
      case WAIT:
        msg = wait_msg;
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
      case STEEP: case WAIT: case PRESS: case DONE:
      text_layer_set_background_color(instr_text_layer, GColorWhite);
      text_layer_set_text_color(instr_text_layer, GColorBlack);
      break;
    }
    text_layer_set_text(instr_text_layer, msg);
}

void reset_times_and_update() {
  // defaults: steep 60 seconds, press 10 seconds
  steep_increment = STEEP_INCREMENT;
  press_increment = PRESS_INCREMENT;
  
  steep_interval = get_interval(STEEP_INTERVAL_TYPE);
  press_interval = get_interval(PRESS_INTERVAL_TYPE);
  wait_interval = DEFAULT_WAIT_INTERVAL;

  update();
}


void up_single_click_handler(ClickRecognizerRef recognizer, void* context) {
  int* var = NULL;
  if (state == SET_STEEP) {
      var = &steep_interval;
  } else if (state == SET_PRESS) {
      var = &press_interval;
  }

  if (var != NULL) {
      if (*var > 1) {
        --*var;
        set_interval(STEEP_INTERVAL_TYPE, steep_interval);
        set_interval(PRESS_INTERVAL_TYPE, press_interval);

        update();
      }
  }
}


void down_single_click_handler(ClickRecognizerRef recognizer, void* context) {
  int* var = NULL;
  if (state == SET_STEEP) {
      var = &steep_interval;
  } else if (state == SET_PRESS) {
      var = &press_interval;
  }

  if (var != NULL) {
      if (*var < 20) {
        ++*var;
        set_interval(STEEP_INTERVAL_TYPE, steep_interval);
        set_interval(PRESS_INTERVAL_TYPE, press_interval);

        update();
      }
  }
}

void handle_timer(void* data);

void start_aeropress_timer() {
  steep_increment = STEEP_INCREMENT;
  press_increment = PRESS_INCREMENT;
  
  timer = app_timer_register(steep_increment*1000, handle_timer, NULL);
}

void select_click_handler(ClickRecognizerRef recognizer, void* context) {
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
    case STEEP: case PRESS: case WAIT:
      break;
    case DONE:
      state = SET_STEEP;

      layer_set_frame(bitmap_layer_get_layer(base_bitmap_white),    BASE_BITMAP_REST_POSITION);
      layer_set_frame(bitmap_layer_get_layer(base_bitmap_black),    BASE_BITMAP_REST_POSITION);

      layer_set_frame(bitmap_layer_get_layer(plunger_bitmap), GRECT_OFFSET(layer_get_frame(image_layer), 0, -36));

      reset_times_and_update();
      break;
  }
}


void click_config_provider(Window *window) {
  (void)window;

  window_single_click_subscribe(BUTTON_ID_DOWN,   down_single_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 200, down_single_click_handler);

  window_single_click_subscribe(BUTTON_ID_UP,     up_single_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 200, up_single_click_handler);

  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

void handle_plunger_animation_done(struct Animation *animation, bool finished, void *context) {
}


void handle_timer(void* data) {
  if (state == STEEP) {
    --steep_interval;
    
    if (steep_interval == 0) {
      ++state;
      vibes_short_pulse();
      
      timer = app_timer_register(wait_increment*1000, handle_timer, NULL);
    } else {
      timer = app_timer_register(steep_increment*1000, handle_timer, NULL);
    }
  } else if (state == WAIT) {
    --wait_interval;
    
    if (wait_interval == 0) {
      ++state;
      vibes_short_pulse();

      animation_set_duration((Animation*)prop_animation, press_interval*press_increment*1000);
      animation_schedule((Animation*)prop_animation);

      timer = app_timer_register(press_increment*1000, handle_timer, NULL);      
    } else {
      timer = app_timer_register(wait_increment*1000, handle_timer, NULL);      
    }
  } else if (state == PRESS) {
    --press_interval;
    
    if (press_interval == 0) {
      ++state;
      vibes_short_pulse();
    } else {
      timer = app_timer_register(press_increment*1000, handle_timer, NULL);
    }
  }
  
  update();
}

void handle_init() {
  state = SET_STEEP;

  window = window_create();
  window_stack_push(window, true);
  
  steep_text_layer = text_layer_create(GRect(0, 0, 144, 15));
  text_layer_set_text_alignment(steep_text_layer, GTextAlignmentCenter);
  text_layer_set_text(steep_text_layer, steep_time_buffer);
  text_layer_set_font(steep_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(steep_text_layer));
  
  press_text_layer = text_layer_create(GRect(0, 15, 144, 15));
  text_layer_set_text_alignment(press_text_layer, GTextAlignmentCenter);
  text_layer_set_text(press_text_layer, press_time_buffer);
  text_layer_set_font(press_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(press_text_layer));
  
  instr_text_layer = text_layer_create(GRect(0, 36, 144, 22));
  text_layer_set_text_alignment(instr_text_layer, GTextAlignmentCenter);
  text_layer_set_text(instr_text_layer, steep_msg);
  text_layer_set_font(instr_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(instr_text_layer));

  reset_times_and_update();
  
  image_layer = layer_create(GRECT_OFFSET(layer_get_frame(window_get_root_layer(window)), 0, 32));
  
  base_bitmap_white = bitmap_layer_create(BASE_BITMAP_REST_POSITION);
  bitmap_layer_set_compositing_mode(base_bitmap_white, GCompOpOr);
  base_white_resource = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BASE_WHITE);
  bitmap_layer_set_bitmap(base_bitmap_white, base_white_resource);

  base_bitmap_black = bitmap_layer_create(BASE_BITMAP_REST_POSITION);
  bitmap_layer_set_compositing_mode(base_bitmap_black, GCompOpClear);
  base_black_resource = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BASE_BLACK);
  bitmap_layer_set_bitmap(base_bitmap_black, base_black_resource);

  plunger_bitmap = bitmap_layer_create(GRECT_OFFSET(layer_get_frame(image_layer), 0, -36));
  bitmap_layer_set_bitmap(plunger_bitmap, gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PLUNGER));
  // rotbmp_pair_init_container(RESOURCE_ID_IMAGE_BASE_WHITE, 
  //                            RESOURCE_ID_IMAGE_BASE_BLACK, &base_bitmap);
                     
  layer_add_child(image_layer, bitmap_layer_get_layer(plunger_bitmap));
  layer_add_child(image_layer, bitmap_layer_get_layer(base_bitmap_white));
  layer_add_child(image_layer, bitmap_layer_get_layer(base_bitmap_black));
  
  layer_add_child(window_get_root_layer(window), image_layer);
  
  GRect from_rect = GRECT_OFFSET(layer_get_frame(image_layer), 0, -36);
  GRect to_rect = GRect(from_rect.origin.x, from_rect.origin.y+20, from_rect.size.w, from_rect.size.h);

  prop_animation = property_animation_create_layer_frame(bitmap_layer_get_layer(plunger_bitmap), &from_rect, &to_rect);
  
  AnimationHandlers animation_handlers = {
    .stopped = &handle_plunger_animation_done
  };
  
  animation_set_handlers((Animation*)prop_animation, animation_handlers, NULL);
  
  window_set_click_config_provider(window, (ClickConfigProvider) click_config_provider);
}

void handle_deinit() {
  text_layer_destroy(steep_text_layer);
  text_layer_destroy(press_text_layer);
  text_layer_destroy(instr_text_layer);

  bitmap_layer_destroy(base_bitmap_white);
  bitmap_layer_destroy(base_bitmap_black);
  bitmap_layer_destroy(plunger_bitmap);

  layer_destroy(image_layer);

  animation_destroy((Animation*)prop_animation);

  app_timer_cancel(timer);

  window_destroy(window);
}

int main(void) {
  // timer = app_timer_register()

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
