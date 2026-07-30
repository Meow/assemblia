#include "level.h"

#include <engine/input/asset.h>
#include <iostream>
#include <memory>

void Level::load_json(const std::string path) {
  char real_path[256];

  asset_find(path.c_str(), real_path);

  simdjson::padded_string json;

  try {
    if (this->verbose)
      std::cout << "Loading level JSON..." << std::endl;

    json = simdjson::padded_string::load(real_path);
  } catch (...) {
    std::cout << "Level " << path << " does not exist or is invalid!" << std::endl;
    return;
  }

  this->json_data = this->json_parser.iterate(json);

  try {
    if (this->verbose)
      std::cout << "Loading layers..." << std::endl;

    this->parse_layers(this->json_data["layers"]);

    if (this->verbose)
      std::cout << "Loading tilesets..." << std::endl;

    this->parse_tilesets(this->json_data["tilesets"]);
  } catch (...) {
    std::cout << "Level " << path << " is invalid!" << std::endl;
    return;
  }

  this->level_path = path;
  this->loaded     = true;
}

void Level::parse_layers(simdjson::ondemand::array layers) {
  for (simdjson::ondemand::value layer : layers) {
    if (this->verbose)
      std::cout << "  -> Parsing layer (determining type)..." << std::endl;

    if (layer.find_field_unordered("type") == std::string_view("tilelayer")) {
      if (this->verbose)
        std::cout << "  -> Tile layer, determining if collision..." << std::endl;

      if (layer.find_field_unordered("name") == std::string_view("collision")) {
        if (this->verbose)
          std::cout << "  -> Parsing collision layer..." << std::endl;

        this->parse_collision_layer(layer);
      } else {
        if (this->verbose)
          std::cout << "  -> Parsing background layer..." << std::endl;

        this->parse_background_layer(layer);
      }
    } else if (layer.find_field_unordered("type") == std::string_view("objectgroup")) {
      if (this->verbose)
        std::cout << "  -> Parsing object layer..." << std::endl;

      this->parse_object_layer(layer);
    }
  }
}

void Level::parse_background_layer(simdjson::ondemand::value layer) {
  if (this->verbose)
    std::cout << "    -> Reading chunks..." << std::endl;

  for (simdjson::ondemand::value chunk : layer.find_field_unordered("chunks").get_array()) {
    const i64 height = chunk.find_field("height");
    const i64 width  = chunk.find_field("width");
    const i64 x      = chunk.find_field("x");
    const i64 y      = chunk.find_field("y");
    u32 i            = 0;

    if (this->verbose) {
      std::cout << "    -> Reading chunk at { x: " << x << ", y: " << y << ", width: " << width
                << ", height: " << height << " }" << std::endl;
    }

    for (simdjson::ondemand::value data : chunk.find_field_unordered("data").get_array()) {
      const i64 tileset_id = data.get_int64();

      if (tileset_id > 0) {
        this->background.push_back(
          { .tileset_id = tileset_id, .x = x * 0.1f + (i % width) * 0.1f, .y = y * 0.1f + (i / height) * 0.1f }
        );
      }

      i++;
    }
  }
}

void Level::parse_collision_layer(simdjson::ondemand::value layer) {
  if (this->verbose)
    std::cout << "    -> Reading chunks..." << std::endl;

  for (simdjson::ondemand::value chunk : layer.find_field_unordered("chunks").get_array()) {
    const i64 height = chunk.find_field("height");
    const i64 width  = chunk.find_field("width");
    const i64 x      = chunk.find_field("x");
    const i64 y      = chunk.find_field("y");
    u32 i            = 0;

    if (this->verbose) {
      std::cout << "    -> Reading chunk at { x: " << x << ", y: " << y << ", width: " << width
                << ", height: " << height << " }" << std::endl;
    }

    for (simdjson::ondemand::value data : chunk.find_field_unordered("data").get_array()) {
      const i64 tileset_id = data.get_int64();

      if (tileset_id > 0)
        this->collisions.push_back({ .x = x * 0.1f + (i % width) * 0.1f, .y = y * 0.1f + (i / height) * 0.1f });

      i++;
    }
  }
}

void Level::parse_object_layer(simdjson::ondemand::value layer) {
  if (this->verbose)
    std::cout << "    -> Reading objects..." << std::endl;

  for (simdjson::ondemand::value obj : layer.find_field_unordered("objects").get_array()) {
    const i64 uid      = obj.find_field("gid");
    const f64 height   = obj.find_field("height");
    const u64 id       = obj.find_field("id");
    const f64 width    = obj.find_field("width");
    const f64 x        = obj.find_field("x");
    const f64 y        = obj.find_field("y");
    std::string script = "";

    if (this->verbose) {
      std::cout << "    -> Read object { x: " << x << ", y: " << y << ", width: " << width << ", height: " << height
                << ", id: " << id << ", uid: " << uid << " }" << std::endl;
    }

    try {
      for (simdjson::ondemand::value props : obj.find_field_unordered("properties").get_array()) {
        const std::string_view name = props.find_field_unordered("name");
        const std::string_view t    = props.find_field_unordered("type");

        if (name == "script" && t == "string") {
          const std::string_view val = props.find_field_unordered("value");
          script                     = std::string(val);
        }
      }
    } catch (...) {
      // ...
    }

    this->objects.push_back(
      { .script     = script,
        .uid        = id,
        .tileset_id = uid,
        .width      = (f32)width / 32.0f,
        .height     = (f32)height / 32.0f,
        .x          = (f32)(x + width / 2) / 32.0f * 0.1f - 0.05f,
        .y          = (f32)(y - height / 2) / 32.0f * 0.1f - 0.05f }
    );
  }
}

void Level::parse_tilesets(simdjson::ondemand::array tilesets) {
  if (this->verbose)
    std::cout << "    -> Reading tilesets..." << std::endl;

  for (simdjson::ondemand::value tileset : tilesets) {
    const u64 uid               = tileset.find_field_unordered("firstgid").get_uint64();
    const std::string_view path = tileset.find_field_unordered("source").get_string();

    if (this->verbose)
      std::cout << "    -> Reading tileset { uid: " << uid << ", path: " << path << " }" << std::endl;

    this->load_tileset(uid, path);
  }
}

std::string absolute_path(const std::string str) {
  if (str.rfind("../", 0) != std::string::npos)
    return absolute_path(str.substr(3, str.length()));

  return str;
}

void Level::load_tileset(const u64 uid, const std::string_view path) {
  char real_path[256];

  if (this->verbose)
    std::cout << "      -> Finding asset..." << std::endl;

  asset_find(std::string(path).c_str(), real_path);

  simdjson::padded_string json;

  try {
    if (this->verbose)
      std::cout << "      -> Loading tileset..." << std::endl;

    json = simdjson::padded_string::load(real_path);
  } catch (...) {
    std::cout << "Tileset " << path << " does not exist!" << std::endl;
    return;
  }

  try {
    if (this->verbose)
      std::cout << "      -> Parsing tileset..." << std::endl;

    simdjson::ondemand::document data = this->json_parser.iterate(json);
    const std::string_view image      = data.find_field("image").get_string();
    const u64 height                  = data.find_field("imageheight");
    const u64 width                   = data.find_field("imagewidth");
    const u64 count                   = data.find_field("tilecount");
    const u64 tileheight              = data.find_field("tileheight");
    const u64 tilewidth               = data.find_field("tilewidth");

    LevelTileset ts = { .path            = absolute_path(std::string(image)),
                        .uid             = uid,
                        .count           = count,
                        .aperture_width  = (f32)tilewidth / (f32)width,
                        .aperture_height = (f32)tileheight / (f32)height };

    this->tilesets.push_back(ts);
  } catch (...) {
    std::cout << "Tileset " << path << " is invalid!" << std::endl;
    return;
  }
}

LevelTileset *Level::find_tileset(const u64 uid) {
  LevelTileset *winner = nullptr;
  u64 highest_uid      = 0;

  for (LevelTileset &ts : this->tilesets) {
    if (ts.uid <= uid && ts.uid > highest_uid) {
      highest_uid = ts.uid;
      winner      = &ts;
    }
  }

  return winner;
}

void Level::init() {
  if (!this->loaded)
    return;

  if (this->verbose)
    std::cout << "Initializing level..." << std::endl;

  LUA_EVENT_RUN(this->lua, "level_init");
  lua_pushstring(this->lua, this->level_path.c_str());
  LUA_EVENT_CALL(this->lua, 1, 1);

  if (lua_isboolean(this->lua, -1) && lua_toboolean(this->lua, -1) == 0)
    return;

  if (this->verbose)
    std::cout << "  -> Spawning background" << std::endl;

  for (const LevelBackgroundTile tile : this->background) {
    LevelTileset *ts = this->find_tileset(tile.tileset_id);

    LUA_EVENT_RUN(this->lua, "pre_level_background_ent_create");

    if (ts != nullptr) {
      lua_createtable(this->lua, 0, 6);
      LUA_TABLE_SET_NAMED(this->lua, number, "id", tile.tileset_id);
      LUA_TABLE_SET_NAMED(this->lua, number, "uid", ts->uid);
      LUA_TABLE_SET_NAMED(this->lua, number, "aperture_width", ts->aperture_width);
      LUA_TABLE_SET_NAMED(this->lua, number, "aperture_height", ts->aperture_height);
      LUA_TABLE_SET_NAMED(this->lua, number, "count", ts->count);
      LUA_TABLE_SET_NAMED(this->lua, string, "texture", ts->path.c_str());
    } else {
      lua_pushnil(this->lua);
    }

    LUA_EVENT_CALL(this->lua, 1, 1);

    if (lua_isboolean(this->lua, -1) && lua_toboolean(this->lua, -1) == 0)
      continue;

    Entity *ent = this->ent_manager->ent_create();
    ent->set_ent_class(EntClass::BACKGROUND);
    ent->set_pos(vector_make3(tile.x, tile.y, 0.0f));
    ent->set_active(true);
    ent->set_texture_path(ts != nullptr ? ts->path : "assets/debug/missing.png");

    LUA_EVENT_RUN(this->lua, "level_background_ent_create");
    lua_push_entity(this->lua, ent);

    if (ts != nullptr) {
      const u64 real_id  = tile.tileset_id - ts->uid;
      const u64 row_size = (u64)(1 / ts->aperture_width);

      ent->set_uv_size({ ts->aperture_width, ts->aperture_height });
      ent->set_uv_offset({ ts->aperture_width * (real_id % row_size), ts->aperture_height * (real_id / row_size) });

      lua_createtable(this->lua, 0, 6);
      LUA_TABLE_SET_NAMED(this->lua, number, "id", tile.tileset_id);
      LUA_TABLE_SET_NAMED(this->lua, number, "uid", ts->uid);
      LUA_TABLE_SET_NAMED(this->lua, number, "aperture_width", ts->aperture_width);
      LUA_TABLE_SET_NAMED(this->lua, number, "aperture_height", ts->aperture_height);
      LUA_TABLE_SET_NAMED(this->lua, number, "count", ts->count);
      LUA_TABLE_SET_NAMED(this->lua, string, "texture", ts->path.c_str());
    } else {
      lua_pushnil(this->lua);
    }

    LUA_EVENT_CALL(this->lua, 2, 0);
  }

  if (this->verbose)
    std::cout << "  -> Spawning objects" << std::endl;

  for (const LevelObject obj : this->objects) {
    LevelTileset *ts = this->find_tileset(obj.tileset_id);

    LUA_EVENT_RUN(this->lua, "pre_level_object_create");

    if (ts != nullptr) {
      lua_createtable(this->lua, 0, 6);
      LUA_TABLE_SET_NAMED(this->lua, number, "id", obj.tileset_id);
      LUA_TABLE_SET_NAMED(this->lua, number, "uid", ts->uid);
      LUA_TABLE_SET_NAMED(this->lua, number, "aperture_width", ts->aperture_width);
      LUA_TABLE_SET_NAMED(this->lua, number, "aperture_height", ts->aperture_height);
      LUA_TABLE_SET_NAMED(this->lua, number, "count", ts->count);
      LUA_TABLE_SET_NAMED(this->lua, string, "texture", ts->path.c_str());
    } else {
      lua_pushnil(this->lua);
    }

    LUA_EVENT_CALL(this->lua, 1, 1);

    if (lua_isboolean(this->lua, -1) && lua_toboolean(this->lua, -1) == 0)
      continue;

    Entity *ent = this->ent_manager->ent_create();
    ent->set_pos({ obj.x, obj.y, 0.0f });
    ent->set_scale({ obj.width, obj.height });
    ent->set_texture_path(ts != nullptr ? ts->path : "assets/debug/missing.png");
    ent->set_id(obj.tileset_id);
    ent->set_active(true);

    LUA_EVENT_RUN(this->lua, "level_object_create");
    lua_push_entity(this->lua, ent);

    if (ts != nullptr) {
      const u64 real_id  = obj.tileset_id - ts->uid;
      const u64 row_size = (u64)(1 / ts->aperture_width);

      ent->set_uv_size({ ts->aperture_width, ts->aperture_height });
      ent->set_uv_offset({ ts->aperture_width * (real_id % row_size), ts->aperture_height * (real_id / row_size) });

      lua_createtable(this->lua, 0, 6);
      LUA_TABLE_SET_NAMED(this->lua, number, "id", obj.tileset_id);
      LUA_TABLE_SET_NAMED(this->lua, number, "uid", ts->uid);
      LUA_TABLE_SET_NAMED(this->lua, number, "aperture_width", ts->aperture_width);
      LUA_TABLE_SET_NAMED(this->lua, number, "aperture_height", ts->aperture_height);
      LUA_TABLE_SET_NAMED(this->lua, number, "count", ts->count);
      LUA_TABLE_SET_NAMED(this->lua, string, "texture", ts->path.c_str());
    } else {
      lua_pushnil(this->lua);
    }

    LUA_EVENT_CALL(this->lua, 2, 0);

    lua_push_entity(this->lua, ent);
    lua_setglobal(this->lua, "ENT");

    if (luaL_dostring(this->lua, obj.script.c_str())) {
      std::cout << "Lua error in LevelObject #" << obj.uid << ":" << std::endl;

      if (lua_isstring(this->lua, -1)) {
        const char *err = luaL_checkstring(this->lua, -1);

        std::cout << err << std::endl;
      } else {
        std::cout << "(error not on stack)" << std::endl;
      }
    }

    lua_pushnil(this->lua);
    lua_setglobal(this->lua, "ENT");
  }

  lua_pushnil(this->lua);
  lua_setglobal(this->lua, "ENT");

  LUA_EVENT_RUN(this->lua, "level_should_spawn_collission");
  LUA_EVENT_CALL(this->lua, 0, 1);

  if (lua_isboolean(this->lua, -1) && lua_toboolean(this->lua, -1) == 0)
    return;

  if (this->verbose)
    std::cout << "  -> Spawning collisions" << std::endl;

  for (const LevelCollisionTile tile : this->collisions) {
    Entity *ent = this->ent_manager->ent_create();
    ent->set_ent_class(EntClass::CLIP);
    ent->set_texture_path("assets/debug/collide_32x32.png");
    ent->set_pos(vector_make3(tile.x, tile.y, 0.0f));
    ent->set_visible(false);
    ent->set_active(true);
  }
}

void Level::update() {}
