#include "game.hpp"
#include "potions.hpp"
#include "title.hpp"
#include "pause.hpp"

Game::Game(SDL_Renderer *renderer)
     : renderer(renderer), current_level(*this), media(*this) {
  // FIXME: Figure out how to determine max collide layers
  collide_layers.resize(1);
  death_fade = {0, 0, 1260, 960};
}

void Game::tick() {
  SDL_SetRenderTarget(renderer, nullptr);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  for (auto &collide_layer : collide_layers) {
    collide_layer.clear();
  }

  if (!paused) {
    current_level.add_colliders(collide_layers);
  }

  for (auto &sprite : sprite_list) {
    if (!paused || sprite->never_paused()) {
      sprite->add_colliders();
    }
  }

  for (auto &sprite : sprite_list) {
    if (!paused || sprite->never_paused()) {
      sprite->tick();
    }
  }

  if (!paused) {
    current_level.handle_reactions();

    current_level.draw();
  }

  
  for (auto &sprite : sprite_list) {
    if (!paused || sprite->never_paused()) {
      sprite->draw();
    }
  }

  if (!paused) {
    // set_cam_trans();
    camera->calc_offset();
    camera->calc_zoom();
  }
  
  std::vector<std::shared_ptr<Sprite>> next_sprite_list;

  for (auto &sprite : sprite_list) {
    if (sprite->is_spawned()) {
      next_sprite_list.push_back(std::move(sprite));
    }
  }

  sprite_list = std::move(next_sprite_list);

  if (!paused && player && player->despawn_time > 0) {
    // fade to black
    opacity =
      (90 - (player->despawn_time - frame_counter.rendered_frames)) * 2.8333;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, (uint8_t)opacity);
    SDL_RenderFillRect(renderer, &death_fade);
    // display death text
    Text t((char *) "You Died", *this, player->get_pos());
    if (frame_counter.rendered_frames == (uint64_t)player->despawn_time-89)
      sprite_list.push_back(std::make_shared<Text>(t));
    t.draw();
    if (frame_counter.rendered_frames == (uint64_t)player->despawn_time) {
      reset_level();
      player->despawn_time = -1;
    }
  }
  if (!paused && player && player->despawn_time < 0) {
    // fade back in
    opacity = 255 + (player->despawn_time * 8.5);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, (uint8_t)opacity);
    SDL_RenderFillRect(renderer, &death_fade);
    player->despawn_time--;
    if (player->despawn_time < -30) {
      player->despawn_time = 0;
    }
  }

  if (!next_level.empty()) {
    load_level(std::filesystem::path(next_level));
  }

  //FIXME: Level load debug
  if (keyboard.is_pressed(SDL_SCANCODE_0)) {
    load_level("data/level/level_1.tmj");
  } else if (keyboard.is_pressed(SDL_SCANCODE_9)) {
    load_level("data/level/test_room_2.tmj");
  } else if (keyboard.is_pressed(SDL_SCANCODE_EQUALS)) {
    reset_level();
  }

  if (!paused) {
    frame_counter.rendered_frames++;
  }

  if (player && !paused && keyboard.is_pressed(SDL_SCANCODE_ESCAPE)) {
    paused = true;
    std::vector<char> curr_screen;
    int pitch = WIDTH * 3;
    curr_screen.resize(pitch * HEIGHT);
    SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGB24, curr_screen.data(), pitch);
    SDL_Texture* background = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, WIDTH, HEIGHT);
    SDL_UpdateTexture(background, nullptr, curr_screen.data(), pitch);

    sprite_list.push_back(std::make_shared<PauseScreen>(*this, background));
  }

  if (should_unpause) {
    paused = false;
    should_unpause = false;
  }

  keyboard.reset_pressed();

  SDL_RenderPresent(renderer);
  auto frame_counter_lock = std::lock_guard(this->frame_counter_lock);
  frame_counter.real_rendered_frames++;
}

void Game::reset_level() {
  load_level(level_path);
}

void Game::load_level(const std::filesystem::path &path) {
  Mix_HaltMusic();
  Mix_HaltChannel(-1);
  sprite_list.clear();
  player = nullptr;
  inventory = nullptr;
  next_level = std::filesystem::path();
  paused = false;
  should_unpause = false;
  keyboard.reset_pressed();
  camera = std::make_shared<Camera>(*this);
  if (!camera) {
    std::cerr << "Camera not initialized" << std::endl;
    abort();
  }

  std::filesystem::path normal_path = path.lexically_normal();
  if (level_path != normal_path) {
    level_path = normal_path;
    media.unloadAll();
  }
  current_level = media.read<LevelFactory>(normal_path)->copy_level();
  for (unsigned layer_id = 0; layer_id < current_level.size();
       layer_id++) {
    for (unsigned y = 0; y < current_level[layer_id].size(); y++) {
      for (unsigned x = 0; x < current_level[layer_id][y].size(); x++) {
        Pos pos;
        pos.layer = static_cast<int>(layer_id);
        pos.y = static_cast<int>(y) * TILE_SUBPIXEL_SIZE;
        pos.x = static_cast<int>(x) * TILE_SUBPIXEL_SIZE;
        if (current_level[pos].props().spawn_type ==
            SpriteSpawnType::PLAYER) {
          Pos player_pos = pos;
          // FIXME: Collide layer placeholder
          player_pos.layer = 0;
          player = std::make_shared<Player>(*this, player_pos);
          break;
        }
      }
    }
  }

  for (unsigned layer_id = 0; layer_id < current_level.size();
       layer_id++) {
    for (unsigned y = 0; y < current_level[layer_id].size(); y++) {
      for (unsigned x = 0; x < current_level[layer_id][y].size(); x++) {
        Pos pos;
        pos.layer = static_cast<int>(layer_id);
        pos.y = static_cast<int>(y) * TILE_SUBPIXEL_SIZE;
        pos.x = static_cast<int>(x) * TILE_SUBPIXEL_SIZE;
        Pos sprite_pos = pos;
        // FIXME: Collide layer placeholder
        sprite_pos.layer = 0;
        if (current_level[pos].props().spawn_type ==
            SpriteSpawnType::CREEP) {
          sprite_list.push_back(std::make_shared<Creep>(*this, sprite_pos));
        } else if (current_level[pos].props().spawn_type ==
            SpriteSpawnType::HEALTH_POTION) {
          sprite_list.push_back(
              std::make_shared<HealthPotion>(*this, sprite_pos));
        } else if (current_level[pos].props().spawn_type ==
            SpriteSpawnType::SPEED_POTION) {
          sprite_list.push_back(
              std::make_shared<SpeedPotion>(*this, sprite_pos));
        } else if (current_level[pos].props().spawn_type ==
            SpriteSpawnType::TITLE_SCREEN) {
          sprite_list.push_back(
              std::make_shared<TitleScreen>(*this));
        }
      }
    }
  }

  if (player) {
    camera->add_focus(player);

    inventory = std::make_shared<Inventory>(*this, Pos { 0, 0, 0 });
    sprite_list.push_back(inventory);

    sprite_list.push_back(player);
    if (level_path == "data/level/level_1.tmj") {
      sprite_list.push_back(std::make_shared<AppearingText>(AppearingText((char*)"A Lever!", *this, Pos{.layer = 0, .x = 58*32*SUBPIXELS_IN_PIXEL, .y = 17*32*SUBPIXELS_IN_PIXEL}, 2, false, {255, 255, 255})));
      sprite_list.push_back(std::make_shared<AppearingText>(AppearingText((char*)"A Secret Room!", *this, Pos{.layer = 0, .x = 27*32*SUBPIXELS_IN_PIXEL, .y = 7*32*SUBPIXELS_IN_PIXEL}, 2, false, {255, 255, 255})));
    }
  }



  /*game.sprite_list.push_back(
      std::make_shared<Text>(Text((char *)"Welcome to Dark Scrolls", game,
                                  Pos{.layer = 0,
                                      .x = 220 * SUBPIXELS_IN_PIXEL,
                                      .y = -27 * SUBPIXELS_IN_PIXEL})));
  game.sprite_list.push_back(std::make_shared<Incantation>(Incantation(
      "This_is_an_incantation", game, Pos{.layer = 0, .x = 0, .y = 100})));*/


  // music
  if (!current_level.get_props().background_music.empty()) {
    Mix_Music *m = media.readMusic(current_level.get_props().background_music);
    Mix_PlayMusic(m, 100);
  }
}

#ifdef DARK_SCROLLS_WINDOWS
std::filesystem::path Game::calc_data_path() {
  return std::filesystem::path(_pgmptr).parent_path() / "data";
}
#endif

#ifdef DARK_SCROLLS_LINUX
std::filesystem::path Game::calc_data_path() {
  return std::filesystem::read_symlink("/proc/self/exe").parent_path() / "data";
}
#endif
