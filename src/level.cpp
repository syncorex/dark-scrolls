#include <unordered_map>
#include <cstdint>
#include <cstddef>
#include <fstream>
#include <filesystem>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <iostream>
#include "level.hpp"

using json = nlohmann::json;

Tile::Tile(SDL_Renderer* renderer, const std::filesystem::path& tileset_loc, uint32_t id, const json& j) {
  std::string texture_path = j["image"];
  std::string texture_fullpath = (tileset_loc.parent_path() / texture_path).u8string();
  SDL_Texture* tex = IMG_LoadTexture(renderer, texture_fullpath.c_str());
  if (!tex) {
    printf("Tile load failed: %s\n", IMG_GetError());
    abort();
  }
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
  SDL_Texture* big_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 32, 32);
  SDL_SetTextureBlendMode(big_tex, SDL_BLENDMODE_BLEND);
  SDL_SetRenderTarget(renderer, big_tex);
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
  SDL_RenderCopy(renderer, tex, nullptr, nullptr);
  SDL_DestroyTexture(tex);
  tex = big_tex;

  auto properties_iter = j.find("properties");
  std::vector<json> props;
  if (properties_iter != j.end()) {
    props = *properties_iter;
  }
  for (auto& prop: props) {
    const std::string& name = prop["name"];
    json value = prop["value"];
    if (name == "invisible") {
      this->properties.invisible = value == true;
    } else if (name == "spawn_tile") {
      if (value == "player") {
        this->properties.spawn_type = SpriteSpawnType::PLAYER;
      } else if (value == "creep") {
        this->properties.spawn_type = SpriteSpawnType::CREEP;
      }
    } else if (name == "activators") {
      std::vector<json> hitboxes = json::parse(std::string(value));
      for (auto& hitbox_json: hitboxes) {
        int activator = 0x1;
        int start_x = 0;
        int width = 32;
        int start_y = 0;
        int height = 32;

        auto activator_iter = hitbox_json.find("type");
        if (activator_iter != hitbox_json.end()) {
          activator = *activator_iter;
        }
        auto start_x_iter = hitbox_json.find("start_x");
        if (start_x_iter != hitbox_json.end()) {
          start_x = *start_x_iter;
        }
        auto width_iter = hitbox_json.find("width");
        if (width_iter != hitbox_json.end()) {
          width = *width_iter;
        }
        auto start_y_iter = hitbox_json.find("start_y");
        if (start_y_iter != hitbox_json.end()) {
          start_y = *start_y_iter;
        }
        auto height_iter = hitbox_json.find("height");
        if (height_iter != hitbox_json.end()) {
          height = *height_iter;
        }

        auto activator_type = static_cast<ActivatorCollideType>(activator);

        start_x *= SUBPIXELS_IN_PIXEL;
        width *= SUBPIXELS_IN_PIXEL;
        start_y *= SUBPIXELS_IN_PIXEL;
        height *= SUBPIXELS_IN_PIXEL;
        this->properties.colliders.activators.push_back(
            ActivatorCollideBox(activator_type, start_x, width, start_y, height));
      }
    }
  }

  this->renderer = renderer;
  this->texture = tex;
  this->id = id;
  backup_texture();
}
Tile::Tile(SDL_Renderer* renderer, SDL_Texture* texture, uint32_t id, TileProperties properties) {
  this->texture = texture;
  this->renderer = renderer;
  this->id = id;
  this->properties = properties;
  backup_texture();
}
Tile Tile::horizontal_flip() {
  SDL_Texture* fliped = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 32, 32);
  SDL_SetTextureBlendMode(fliped, SDL_BLENDMODE_BLEND);
  SDL_SetRenderTarget(renderer, fliped);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
  SDL_RenderCopyEx(renderer, texture, nullptr, nullptr, 0, nullptr, SDL_FLIP_HORIZONTAL);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  
  TileProperties props = properties;
  for (auto& activator: props.colliders.activators) {
    activator.offset_x = 32 - activator.offset_x;
  }

  return Tile(renderer, fliped, id | 0x80000000, std::move(props));
}
Tile Tile::vertical_flip() {
  SDL_Texture* fliped = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 32, 32);
  SDL_SetTextureBlendMode(fliped, SDL_BLENDMODE_BLEND);
  SDL_SetRenderTarget(renderer, fliped);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
  SDL_RenderCopyEx(renderer, texture, nullptr, nullptr, 0, nullptr, SDL_FLIP_VERTICAL);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

  TileProperties props = properties;
  for (auto& activator: props.colliders.activators) {
    activator.offset_y = 32 - activator.offset_y;
  }

  return Tile(renderer, fliped, id | 0x40000000, std::move(props));
}
Tile Tile::diagonal_flip() {
  SDL_Texture* fliped = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 32, 32);
  SDL_SetTextureBlendMode(fliped, SDL_BLENDMODE_BLEND);
  SDL_SetRenderTarget(renderer, fliped);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
  SDL_RenderCopyEx(renderer, texture, nullptr, nullptr, 90, nullptr, SDL_FLIP_VERTICAL);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

  TileProperties props = properties;
  for (auto& activator: props.colliders.activators) {
    // Flip x and y axis
    activator = ActivatorCollideBox(
      activator.type, activator.offset_y, activator.height, 
      activator.offset_x, activator.width
    );
  }

  return Tile(renderer, fliped, id | 0x20000000, std::move(props));
}

Tile::Tile(const Tile& other) noexcept {
  this->properties = other.properties;
  this->renderer = other.renderer;
  this->id = other.id;
  this->texture_backup = other.texture_backup;

  this->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 32, 32);
  SDL_SetTextureBlendMode(this->texture, SDL_BLENDMODE_BLEND);
  SDL_Texture* old_render_target = SDL_GetRenderTarget(renderer);
  SDL_SetRenderTarget(renderer, this->texture);
  SDL_SetTextureBlendMode(other.texture, SDL_BLENDMODE_NONE);
  SDL_RenderCopy(renderer, other.texture, nullptr, nullptr);
  SDL_SetTextureBlendMode(other.texture, SDL_BLENDMODE_BLEND);
  SDL_SetRenderTarget(renderer, old_render_target);
}

void Tile::backup_texture() {
  SDL_SetRenderTarget(renderer, texture);
  texture_backup.resize(32 * 32);
  SDL_Rect backup_zone = {.x = 0, .y = 0, .w = 32, .h = 32};
  SDL_RenderReadPixels(renderer, &backup_zone, SDL_PIXELFORMAT_RGBA8888, &texture_backup.front(), 32 * 4);
}

void Tile::reload_texture() {
  SDL_DestroyTexture(texture);
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, 32, 32);
  SDL_UpdateTexture(texture, nullptr, &texture_backup.front(), 32 * 4);
}

Layer::Layer(std::vector<std::vector<std::shared_ptr<Tile>>> tiles) {
  tile_data = std::move(tiles);
}

Level::Level(SDL_Renderer* renderer) {
  this->width = 0;
  this->height = 0;
  this->renderer = renderer;
}
Level::Level(SDL_Renderer* renderer, const std::filesystem::path& level_loc) {
  std::ifstream level_file(level_loc);
  json level_data = json::parse(level_file);
  this->width = level_data["width"].get<uint32_t>();
  this->height = level_data["height"].get<uint32_t>();
  this->renderer = renderer;
  std::shared_ptr<Tile> empty = std::make_shared<Tile>(Tile(renderer, nullptr, 0, TileProperties()));
  empty->props().invisible = true;
  tilemap.insert(std::pair(0, std::move(empty)));

  std::vector<json> tilesets = level_data["tilesets"];
  std::sort(tilesets.begin(), tilesets.end(), [](const json& lhs, const json& rhs) {
    return lhs["firstgid"].get<uint32_t>() < rhs["firstgid"].get<uint32_t>();
  });
  for (size_t i = 0; i < tilesets.size(); i++) {
    uint32_t first_tid = tilesets[i]["firstgid"].get<uint32_t>();
    uint32_t last_tid = UINT32_MAX;
    if (i < tilesets.size() - 1) {
      last_tid = tilesets[i + 1]["firstgid"].get<uint32_t>();
    }
    std::filesystem::path tileset_loc = level_loc.parent_path() / tilesets[i]["source"].get<std::string>();
    std::ifstream tileset_file(tileset_loc);
    json tileset_json = json::parse(tileset_file);
    load_tileset(tileset_json, tileset_loc, first_tid, last_tid);
  }

  std::vector<json> layers = level_data["layers"];
  for (auto& layer_data: layers) {
    std::vector<std::vector<std::shared_ptr<Tile>>> tiles;
    std::vector<std::shared_ptr<Tile>> current_row;
    std::vector<uint32_t> tile_data = layer_data["data"].get<std::vector<uint32_t>>();
    for (uint32_t tile_id: tile_data) {
      tile_id &= ~0x10000000; // This flip bit is only used for hex maps and should be masked for non-hex maps.
      auto tile_iter = tilemap.find(tile_id);
      if (tile_iter == tilemap.end()) {
        std::cerr << "Tile " << tile_id << " could not be found." << std::endl;
        abort();
      }
      std::shared_ptr<Tile> tile = tile_iter->second;
      current_row.push_back(tile);
      if (current_row.size() >= this->width) {
        tiles.push_back(std::move(current_row));
        current_row.clear();
      }
    }
    this->layers.push_back(Layer(std::move(tiles)));
  }
}

void Level::draw() {
  for (auto& layer: layers) {
    int camera_offset_x = camera_offset.x / SUBPIXELS_IN_PIXEL;
    int camera_offset_y = camera_offset.y / SUBPIXELS_IN_PIXEL;
    SDL_Rect dest_loc = {camera_offset_x, camera_offset_y, 32, 32};
    for (auto& row: layer) {
      for (auto& tile: row) {
        if (!tile->props().invisible) {
          SDL_RenderCopy(renderer, tile->get_texture(), nullptr, &dest_loc);
        }
        dest_loc.x += 32;
      }
      dest_loc.x = camera_offset_x;
      dest_loc.y += 32;
    }
  }
}
void Level::load_tileset(const json& tileset, const std::filesystem::path& tileset_loc, uint32_t first_tid, uint32_t end_tid) {
  std::vector<json> tiles = tileset["tiles"];
  for (auto& tile_data: tiles) {
    uint32_t global_tile_id = tile_data["id"].get<uint32_t>() + first_tid;
    if (global_tile_id > end_tid) {
      continue;
    }
    Tile base_tile(renderer, tileset_loc, global_tile_id, tile_data);

    for (int flip_bits = 0; flip_bits < 0x8; flip_bits++) {
      Tile new_tile = base_tile;
      if ((flip_bits & 0x1) == 0x1) {
        new_tile = new_tile.diagonal_flip();
      }
      if ((flip_bits & 0x2) == 0x2) {
        new_tile = new_tile.horizontal_flip();
      }
      if ((flip_bits & 0x4) == 0x4) {
        new_tile = new_tile.vertical_flip();
      }
      tilemap.insert(std::pair(new_tile.get_id(), std::make_shared<Tile>(std::move(new_tile))));
    }
  }
}

void Level::reload_texture() {
  for (auto& tile_pair: tilemap) {
    tile_pair.second->reload_texture();
  }
}

void Level::add_colliders(std::vector<CollideLayer>& collide_layers) {
  for (auto& tile_layer: *this) {
    for (size_t y = 0; y < tile_layer.size(); y++) {
      auto& tile_row = tile_layer[y];
      for (size_t x = 0; x < tile_row.size(); x++) {
        Tile& tile = *tile_row[x];
        for (auto& activator: tile.props().colliders.activators) {
          Pos pos = {.layer = 0, .x = static_cast<int>(x * TILE_SUBPIXEL_SIZE), 
            .y = static_cast<int>(y * TILE_SUBPIXEL_SIZE)
          };
          //TODO: Assumes collide layer is always zero
          collide_layers[0].add_activator(activator, pos);
        }
      }
    }
  }
}
