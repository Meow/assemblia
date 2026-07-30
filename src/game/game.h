#ifndef __GAME_GAME_H
#define __GAME_GAME_H

#define GAME_GETTER(t, n) \
  t get_##n() const { return this->n; }

#include "controllers/battle_controller.h"
#include "controllers/dungeon_controller.h"
#include "controllers/menu_controller.h"
#include "controllers/overworld_controller.h"
#include "entities/entity.h"
#include "entities/entity_manager.h"
#include "entities/player.h"
#include "level.h"
#include "lua/entity.h"
#include "lua/helpers.h"
#include "lua/text.h"
#include "text.h"

#include <engine/input/input.h>
#include <engine/limits.h>
#include <engine/platform.h>
#include <engine/render/glyph.h>
#include <engine/render/renderable.h>
#include <engine/state/state.h>
#include <engine/types/numeric.h>
#include <lua.hpp>
#include <memory>
#include <vector>

enum GameStage { GS_MENU, GS_OVERWORLD, GS_DUNGEON, GS_BATTLE, GS_UNKNOWN };

class Game {
private:
  EntityManager ent_manager;
  Text text[MAX_TEXT_OBJECTS];
  std::shared_ptr<Level> level;
  GameState *game_state;
  InputState *input_state;
  lua_State *lua;
  OverworldController *overworld_controller;
  DungeonController *dungeon_controller;
  BattleController *battle_controller;
  MenuController *menu_controller;
  Entity *background;
  GameStage stage;
  f32 window_width;
  f32 window_height;
  f32 window_scale_x;
  f32 window_scale_y;
  f32 scale;
  bool locked;
  bool should_clear_text;

public:
  Game() {
    this->input_state          = nullptr;
    this->game_state           = nullptr;
    this->overworld_controller = nullptr;
    this->dungeon_controller   = nullptr;
    this->battle_controller    = nullptr;
    this->menu_controller      = nullptr;
    this->scale                = RENDER_SCALE;
    this->stage                = GameStage::GS_UNKNOWN;
    this->locked               = false;
    this->should_clear_text    = false;
    this->window_width         = 1280.0f;
    this->window_height        = 720.0f;
    this->window_scale_x       = 1.0f;
    this->window_scale_y       = 1.0f;
  }

  GAME_GETTER(InputState *, input_state);
  GAME_GETTER(GameStage, stage);
  GAME_GETTER(f32, scale);
  GAME_GETTER(f32, window_width);
  GAME_GETTER(f32, window_height);
  GAME_GETTER(f32, window_scale_x);
  GAME_GETTER(f32, window_scale_y);

  /* Events called from the engine */
  void init(GameState *state);
  void update(GameState *state, mutex_t *lock);
  void update_lazy(GameState *state, mutex_t *lock);
  void update_paused(GameState *state, mutex_t *lock);
  void update_renderables(
    GameState *state,
    mutex_t *lock,
    RenderState *render_state,
    Renderable **renderables,
    const u32 renderables_count,
    GlyphText *text_objects,
    const u32 text_objects_count
  );
  void create_bindings(GameState *state, mutex_t *lock, InputState *input_state);
  void process_input(GameState *state, const f64 update_time);
  void set_stage(const GameStage stage);
  void init_lua();
  void ent_remove(Entity *ent);
  void clear_entities();
  void changelevel(const std::string level);
  Text *add_text(const std::string text, const Vector4 pos, const Vector4 color, const Vector2 scale);
  void clear_text();

  static bool compare_renderables(const Renderable *r1, const Renderable *r2);
  static i32 lua_set_stage(lua_State *state);
  static i32 lua_bind(lua_State *state);
  static i32 lua_get_stage(lua_State *state);
  static i32 lua_player(lua_State *state);
  static i32 lua_getscale(lua_State *state);
  static i32 lua_ent_create(lua_State *state);
  static i32 lua_ent_clear(lua_State *state);
  static i32 lua_ent_remove(lua_State *state);
  static i32 lua_changelevel(lua_State *state);
  static i32 lua_window_getheight(lua_State *state);
  static i32 lua_window_getwidth(lua_State *state);
  static i32 lua_window_getscale(lua_State *state);
  static i32 lua_add_text(lua_State *state);
  static i32 lua_clear_text(lua_State *state);
};

#endif
