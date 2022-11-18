#pragma once
#include "player.hpp"
#include "keyboard_manager.hpp"
#include "collide.hpp"
#include "camera.hpp"
#include "media_manager.hpp"
#include <mutex>

constexpr double FRAME_RATE = 1.0 / 60.0;

struct FrameCounter {
  uint64_t rendered_frames = 0;
  uint64_t scheduled_frames = 0;
};

class Player;

class Game: public std::enable_shared_from_this<Game> {
  public:
  Game(SDL_Renderer *renderer): renderer(renderer), current_level(*this), media(renderer) {
    // FIXME: Figure out how to determine max collide layers
    collide_layers.resize(1);
  }

  std::shared_ptr<Camera> camera = NULL;
  SDL_Renderer *renderer;
  KeyboardManager keyboard;
  int32_t tick_event_id;
  FrameCounter frame_counter;
  std::mutex frame_counter_lock;
  Level current_level;
  std::filesystem::path data_path = calc_data_path();
  std::vector<std::shared_ptr<Sprite>> sprite_list;
  std::shared_ptr<Player> player = NULL;
  std::vector<CollideLayer> collide_layers;
  MediaManager media;

  //Text test_text;

  void tick();
  private:
  static std::filesystem::path calc_data_path();
};
