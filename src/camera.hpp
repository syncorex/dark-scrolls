#pragma once
#include "pos.hpp"
#include "sprite.hpp"
#include <SDL2/SDL.h>
#include <memory>
#include <vector>

class Game;

struct FocusPoint {
  Pos &pos;
  float weight;
  FocusPoint(Pos &npos, float nweight);
};

class Camera {
private:
  Game &game;
  std::vector<std::shared_ptr<Sprite>> focus_points;
  Translation offset = {0, 0};
  Translation eased_offset = offset;
  float zoom_default;
  float zoom_factor;
  const float max_zoom = 3;

  SDL_Rect rect_offsetted(SDL_Rect r);
  SDL_Rect rect_scaled(SDL_Rect r);
  SDL_Rect rect_processed(SDL_Rect r);

public:
  bool zoom_enabled;
  Camera(Game &game);
  void calc_offset();
  void calc_eased_offset();
  void calc_zoom();
  void add_focus(std::shared_ptr<Sprite> sprite);

  /*
  Wrappers for SDL render functions that apply zoom and offset.

  NOTE: These functions expect dstrect to have been scaled with
  SUBPIXELS_IN_PIXEL. Only render sprites with these functions.
  */

  int render(SDL_Renderer *renderer, SDL_Texture *texture,
             const SDL_Rect *srcrect, const SDL_Rect *dstrect);

  int render_ex(SDL_Renderer *renderer, SDL_Texture *texture,
                const SDL_Rect *srcrect, SDL_Rect *dstrect, const double angle,
                const SDL_Point *center, const SDL_RendererFlip flip);

  int fill_rect(SDL_Renderer *renderer, SDL_Rect *dstrect);
};