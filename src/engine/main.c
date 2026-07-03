#include "input/argparse.h"
#include "input/input.h"
#include "logic/logic.h"
#include "platform.h"
#include "render/render.h"
#include "state/state.h"
#include "util.h"

#include <game/interface.h>

/**
 * @brief The main entry point of the engine executable
 *
 * @param argc Amount of command-line parameters passed
 * @param argv List of parameters separated by space, including executable name
 * itself
 * @return i32 Exit status code
 */
i32 main(const i32 argc, const char *argv[]) {
  printf("[Miniflow] Starting...\n");

  /* Initialize game state and a mutex lock */
  GameState *const state = game_default_state();
  mutex_t state_lock;

  input_parse_args(state, argc, argv);

  if (game_is_debug(state))
    DEBUG_MESSAGE("Debug mode is enabled.\n");

  game_initialize(state);

  /* Spawn threads */
  ThreadData thread_data = { .state = state, .lock = &state_lock };

#ifdef __APPLE__
  /* macOS (Cocoa) requires that windowing and event polling happen on the main
   * thread, so the render thread — which owns the GLFW window and pumps events
   * — must be the main thread here. Only logic and input are spawned. */
  const thread_t logic_thread = platform_spawn(logic_perform, &thread_data);
  const thread_t input_thread = platform_spawn(input_perform, &thread_data);

  DEBUG_MESSAGE("Entering main loop...\n");

  render_perform(&thread_data);

  printf("Exiting...\n");

  platform_join(input_thread);
  platform_join(logic_thread);
#else
  const thread_t render_thread = platform_spawn(render_perform, &thread_data);
  const thread_t logic_thread  = platform_spawn(logic_perform, &thread_data);
  const thread_t input_thread  = platform_spawn(input_perform, &thread_data);

  DEBUG_MESSAGE("Entering main loop...\n");

  /* Wait until game state tells us we should exit */
  while (!game_should_exit(state))
    platform_sleep(1000);

  printf("Exiting...\n");

  /* Join all threads into the main thread */
  platform_join(input_thread);
  platform_join(logic_thread);
  platform_join(render_thread);
#endif

  /* Clean up */
  free(state);

  return 0;
}
