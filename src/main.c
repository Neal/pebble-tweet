#include <pebble.h>
#include "setup.h"
#include "gdraw_command_transforms.h"

#define PERSIST_KEY_HAS_TOKENS (100)

enum {
  APP_KEY_TEXT,
  APP_KEY_SUCCESS,
  APP_KEY_HAS_TOKENS,
  APP_KEY_QUIT,
};

static Window *s_window;
static Layer *s_logo_layer;
static GDrawCommandImage *s_logo;
static DictationSession *s_dictation_session;
static Animation *s_current_animation;
static bool s_has_tokens;
static uint32_t s_to_square_normalized = ANIMATION_NORMALIZED_MAX;

static inline int32_t distance_interpolate(const int32_t normalized, int32_t from, int32_t to) {
  return from + (((int64_t)normalized * (to - from)) / (int64_t)ANIMATION_NORMALIZED_MAX);
}

static void dictation_start_timer_callback(void *data) {
  if (s_has_tokens) {
    dictation_session_start(s_dictation_session);
  }
}

static void update_icon_square_normalized(Animation *animation, const uint32_t distance_normalized) {
  s_to_square_normalized = distance_normalized;
  layer_mark_dirty(s_logo_layer);
}

static const PropertyAnimationImplementation s_icon_square_normalized_implementation = {
  .base = {
    .update = (AnimationUpdateImplementation) update_icon_square_normalized,
  },
};

static void to_dot_stopped_handler(Animation *animation, bool finished, void *context) {
  window_stack_pop_all(true);
}

static void animate_to_dot() {
  Animation *anim = (Animation *) property_animation_create(&s_icon_square_normalized_implementation, NULL, NULL, NULL);
  animation_set_duration(anim, 80);
  animation_set_curve(anim, AnimationCurveEaseOut);
  animation_set_handlers(anim, (AnimationHandlers) {
    .stopped = to_dot_stopped_handler,
  }, NULL);
  animation_schedule(anim);
  s_current_animation = anim;
}

static void from_dot_stopped_handler(Animation *animation, bool finished, void *context) {
  app_timer_register(0, dictation_start_timer_callback, NULL);
}

static void animate_from_dot() {
  Animation *anim = (Animation *) property_animation_create(&s_icon_square_normalized_implementation, NULL, NULL, NULL);
  animation_set_duration(anim, 240);
  animation_set_curve(anim, AnimationCurveEaseIn);
  animation_set_reverse(anim, true);
  animation_set_handlers(anim, (AnimationHandlers) {
    .stopped = from_dot_stopped_handler,
  }, NULL);
  animation_schedule(anim);
  s_current_animation = anim;
}

static void app_quit_timer_callback(void *data) {
  animate_to_dot();
}

static void inbox_received_callback(DictionaryIterator *iter, void *context) {
  Tuple *t = dict_find(iter, APP_KEY_HAS_TOKENS);
  if (t) {
    s_has_tokens = t->value->int8;
    persist_write_bool(PERSIST_KEY_HAS_TOKENS, s_has_tokens);
    if (!s_has_tokens) {
      setup_window_push();
    } else {
      setup_window_pop();
      if (!animation_is_scheduled(s_current_animation)) {
        app_timer_register(100, dictation_start_timer_callback, NULL);
      }
    }
  }

  if (dict_find(iter, APP_KEY_SUCCESS)) {
    app_timer_register(200, app_quit_timer_callback, NULL);
  }
}

static void prv_result_handler(DictationSession *session, DictationSessionStatus result,
                               char *transcription, void *context) {
  if (result != DictationSessionStatusSuccess) {
    return;
  }

  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_cstring(iter, APP_KEY_TEXT, transcription);
  dict_write_end(iter);
  app_message_outbox_send();
}

static void prv_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_has_tokens) {
    dictation_session_start(s_dictation_session);
  }
}

static void prv_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  animate_to_dot();
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click_handler);
}

static void layer_update_proc(Layer *layer, GContext *ctx) {
  if (!s_logo) {
    return;
  }

  GRect bounds = layer_get_bounds(layer);

  GSize logo_size = gdraw_command_image_get_bounds_size(s_logo);

  GPoint logo_offset = grect_center_point(&bounds);
  logo_offset.x -= logo_size.w / 2;
  logo_offset.y -= logo_size.h / 2;

  logo_offset.x = distance_interpolate(s_to_square_normalized,
                                       logo_offset.x,
                                       logo_offset.x + (logo_size.w / 2));

  logo_offset.y = distance_interpolate(s_to_square_normalized,
                                       logo_offset.y,
                                       logo_offset.y + (logo_size.h / 2));

  GDrawCommandImage *temp_copy = gdraw_command_image_clone(s_logo);
  attract_draw_command_image_to_square(temp_copy, s_to_square_normalized);
  graphics_context_set_antialiased(ctx, true);
  gdraw_command_image_draw(ctx, temp_copy, logo_offset);
  gdraw_command_image_destroy(temp_copy);
}

static void logo_in_anim_stopped(Animation *animation, bool finished, void *context) {
  app_timer_register(500, dictation_start_timer_callback, NULL);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_logo = gdraw_command_image_create_with_resource(RESOURCE_ID_TWITTER_PDC_LARGE);

  s_logo_layer = layer_create(bounds);
  layer_set_update_proc(s_logo_layer, layer_update_proc);
  layer_add_child(window_layer, s_logo_layer);

  animate_from_dot();
}

static void window_unload(Window *window) {
  layer_destroy(s_logo_layer);
  gdraw_command_image_destroy(s_logo);
}

static void init(void) {
  app_message_register_inbox_received(inbox_received_callback);
  app_message_open(64, 160);

  s_dictation_session = dictation_session_create(140, prv_result_handler, NULL);

  s_has_tokens = persist_read_bool(PERSIST_KEY_HAS_TOKENS);

  s_window = window_create();
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_background_color(s_window, GColorVividCerulean);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  window_stack_push(s_window, true);
}

static void deinit(void) {
  dictation_session_destroy(s_dictation_session);
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
