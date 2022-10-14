#pragma once
#include <SDL2/SDL_mixer.h>
#include "mob.hpp"


class Game;

class Player: public Mob {
  public:
  Player(Game &game, Pos pos);

  bool is_immobile() const {
    return this->IMMOBILE_FLAG;
  }

  void immobile(bool b) {
    this->IMMOBILE_FLAG = b;
  }

  virtual void draw();
  virtual void tick();

  private:
  bool IMMOBILE_FLAG = false;
  bool moving = false;
  bool facing_left = false;
  uint32_t speed;
  SDL_Surface *surface = nullptr;
  SDL_Texture *texture = nullptr;
  Mix_Chunk *walk_sound;
  static constexpr SDL_Rect SHAPE = {.x = 0, .y = 0, .w = 37, .h = 37};
  static constexpr uint8_t RED = 126;
  static constexpr uint8_t GREEN = 219;
  static constexpr uint8_t BLUE = 222;
};
