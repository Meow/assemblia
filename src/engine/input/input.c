#include "input.h"

#include <GLFW/glfw3.h>
#include <game/interface.h>
#include <string.h>

static InputState *IS;

void input_refresh_callback(GLFWwindow *window) {}

void input_resize_callback(GLFWwindow *window, i32 width, i32 height) {}

void input_key_callback(GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods) {
  if (key > -1) {
    i32 real_key = (action << 30) + (mods << 24) + key;

    KeyBind bind = input_find_binding(IS, real_key);

    if (bind.cmd != 0)
      input_push(IS, bind.cmd);
  }
}

void input_char_callback(GLFWwindow *window, u32 codepoint) { input_push_raw(IS, codepoint); }

void input_cursor_position_callback(GLFWwindow *window, f64 x, f64 y) {
  IS->mouse.x = x;
  IS->mouse.y = y;
}

void input_mouse_button_callback(GLFWwindow *window, i32 button, i32 action, i32 mods) {
  i32 real_key = (action << 30) + (mods << 24) + (button << 21);
  KeyBind bind = input_find_binding(IS, real_key);

  if (bind.cmd != 0)
    input_push(IS, bind.cmd);
}

i32 input_perform(void *args) {
  ThreadData *const td   = (ThreadData *)args;
  GameState *const state = (GameState *)td->state;
  mutex_t *lock          = (mutex_t *)td->lock;
  GLFWwindow *window     = (GLFWwindow *)state->window;
  IS                     = input_state_default();

  /* Avoid the race condition between render thread and this thread. The render
   * thread may also exit before ever creating a window (e.g. failed shader
   * compilation), in which case we must bail instead of waiting forever. */
  while (window == NULL) {
    if (game_should_exit(state)) {
      input_state_free(IS);

      return 0;
    }

    window = (GLFWwindow *)state->window;
    platform_sleep(1);
  }

  glfwSetWindowRefreshCallback(window, input_refresh_callback);
  glfwSetFramebufferSizeCallback(window, input_resize_callback);
  glfwSetKeyCallback(window, input_key_callback);
  glfwSetCursorPosCallback(window, input_cursor_position_callback);
  glfwSetMouseButtonCallback(window, input_mouse_button_callback);

  game_create_bindings(state, lock, IS);

  GLFWgamepadstate gs_last;

  while (!game_should_exit(state)) {
#if !defined(_WIN32) && !defined(__APPLE__)
    glfwPollEvents();
#endif

    GLFWgamepadstate gs;

    if (glfwGetGamepadState(GLFW_JOYSTICK_1, &gs)) {
      IS->left_stick.x  = gs.axes[0];
      IS->left_stick.y  = gs.axes[1];
      IS->right_stick.x = gs.axes[2];
      IS->right_stick.y = gs.axes[3];
      IS->left_trigger  = gs.axes[4];
      IS->right_trigger = gs.axes[5];

      for (u8 i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; i++) {
        IS->gamepad[i] = gs.buttons[i];

        if (gs.buttons[i] != gs_last.buttons[i]) {
          i32 real_key = (gs.buttons[i] << 30) + (i << 17);
          KeyBind bind = input_find_binding(IS, real_key);

          if (bind.cmd != 0)
            input_push(IS, bind.cmd);
        }
      }
    }

    memcpy(&gs_last, &gs, sizeof(GLFWgamepadstate));

    platform_sleep(2);
  }

  input_state_free(IS);

  return 0;
}
