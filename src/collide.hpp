#pragma once
#include "pos.hpp"
#include "util.hpp"
#include <functional>
#include <vector>

// The biggest dimmentions (in tiles) that collision detection functions
constexpr int MAX_COLLIDE_X = 64;
constexpr int MAX_COLLIDE_Y = 64;

struct ReactorCollideType;

struct ActivatorCollideType : public BitFlag<ActivatorCollideType> {
public:
  using CounterPart = ReactorCollideType;
  constexpr ActivatorCollideType() : BitFlag<ActivatorCollideType>() {}
  explicit constexpr ActivatorCollideType(int raw)
      : BitFlag<ActivatorCollideType>(raw) {}
  constexpr ActivatorCollideType(const BitFlag<ActivatorCollideType> &inner)
      : BitFlag<ActivatorCollideType>(inner) {}

  static const ActivatorCollideType WALL;
  static const ActivatorCollideType HIT_EVIL;
  static const ActivatorCollideType HIT_GOOD;
  static const ActivatorCollideType HIT_ALL;
  static const ActivatorCollideType INTERACT;
  ReactorCollideType activates() const;

  ActivatorCollideType operator&(const ActivatorCollideType &other) const;
  ActivatorCollideType operator&(ReactorCollideType counter_part) const;
};

// There are bitflags (increase in powers of two)
// Symetric with reactor (every member here must have a counter part there)
constexpr ActivatorCollideType
    ActivatorCollideType::WALL(0x1); // Acts like a wall
constexpr ActivatorCollideType
    ActivatorCollideType::HIT_EVIL(0x2); // Hurts evil sprites
constexpr ActivatorCollideType
    ActivatorCollideType::HIT_GOOD(0x4); // Hurts good sprites (probally player)
constexpr ActivatorCollideType ActivatorCollideType::HIT_ALL(
    ActivatorCollideType::HIT_EVIL | ActivatorCollideType::HIT_GOOD);
constexpr ActivatorCollideType ActivatorCollideType::INTERACT(0x8);

struct ReactorCollideType : public BitFlag<ReactorCollideType> {
public:
  using CounterPart = ActivatorCollideType;
  constexpr ReactorCollideType() : BitFlag<ReactorCollideType>() {}
  explicit constexpr ReactorCollideType(int raw)
      : BitFlag<ReactorCollideType>(raw) {}
  constexpr ReactorCollideType(const BitFlag<ReactorCollideType> &inner)
      : BitFlag<ReactorCollideType>(inner) {}

  static const ReactorCollideType WALL;
  static const ReactorCollideType HURT_BY_EVIL;
  static const ReactorCollideType HURT_BY_GOOD;
  static const ReactorCollideType HURT_BY_ANY;
  static const ReactorCollideType INTERACTABLE;
  ActivatorCollideType activated_by() const;

  ReactorCollideType operator&(const ReactorCollideType &other) const;
  ReactorCollideType operator&(ActivatorCollideType counter_part) const;
};

// There are bitflags (increase in powers of two)
// Symetric with activator (every member here must have a counter part there)
constexpr ReactorCollideType ReactorCollideType::WALL(0x1); // Affected by walls
constexpr ReactorCollideType
    ReactorCollideType::HURT_BY_GOOD(0x2); // Can be hurt (only by good stuff)
constexpr ReactorCollideType
    ReactorCollideType::HURT_BY_EVIL(0x4); // Can be hurt (only by evil stuff)
constexpr ReactorCollideType ReactorCollideType::HURT_BY_ANY(
    ReactorCollideType::HURT_BY_GOOD | ReactorCollideType::HURT_BY_EVIL);
constexpr ReactorCollideType ReactorCollideType::INTERACTABLE(0x8);

template <typename CollideType> class CollideBox;

using ActivatorCollideBox = CollideBox<ActivatorCollideType>;
using ReactorCollideBox = CollideBox<ReactorCollideType>;

struct CollideDamageProps {
  int hp_delt;
};

using OnCollideRecoilFn = std::function<void(Pos, ReactorCollideBox)>;

extern const OnCollideRecoilFn DO_NOTHING_ON_COLLIDE_RECOIL;

struct ActivatorCollideProps {
  CollideDamageProps damage;
  OnCollideRecoilFn on_recoil = DO_NOTHING_ON_COLLIDE_RECOIL;
};

// CollideType is either ActivatorCollideType or ReactorCollideType
template <typename CollideType>
class CollideBox : public std::conditional_t<
                       std::is_same_v<CollideType, ActivatorCollideType>,
                       ActivatorCollideProps, Empty> {
public:
  CollideBox() : CollideBox(CollideType::WALL, 0, 0, 0, 0) {}
  CollideBox(CollideType type, int offset_x, int width, int offset_y,
             int height) {
    static_assert(std::is_same_v<CollideType, ActivatorCollideType> ||
                  std::is_same_v<CollideType, ReactorCollideType>);
    this->type = type;
    this->offset_x = offset_x;
    this->offset_y = offset_y;
    this->width = width;
    this->height = height;
  }

  bool collides_with(const Pos &here,
                     const CollideBox<typename CollideType::CounterPart> &other,
                     const Pos &there) const {
    int this_x = here.x + this->offset_x;
    int this_y = here.y + this->offset_y;
    int other_x = there.x + other.offset_x;
    int other_y = there.y + other.offset_y;

    return (this->type & other.type) != 0 && here.layer == there.layer &&
           this_x < other_x + other.width && this_x + this->width > other_x &&
           this_y < other_y + other.height && this_y + this->height > other_y;
  }
  CollideType type;
  // The disjoint of the hitbox: the distance from the top left corner of a
  // thing to where its hitbox begins e.g. a swords tip would be slightly ahead
  // of a player
  int offset_x;
  int offset_y;
  int width;
  int height;
};

class CollideLayer {
public:
  CollideLayer();

  void clear();

  void add_activator(ActivatorCollideBox activator, Pos here);

  bool overlaps_activator(ReactorCollideBox react, Pos here,
                          Pos *collide_out = nullptr,
                          ActivatorCollideBox *activator_out = nullptr);

private:
  std::vector<std::vector<std::vector<ActivatorCollideBox>>> colliders;

  // HitBoxType is a valid perameter for CollideBox
  // Func is a function object that can be called as: bool callback(Pos
  // collide_visit) This function will early return if Func returns false, it
  // will continue if Fnc returns true.
  template <typename HitBoxType, typename Func>
  void walk_tiles(const CollideBox<HitBoxType> &hitbox, const Pos &here,
                  Func func) {
    int end_x = here.x + hitbox.width;
    int end_y = here.y + hitbox.height;

    Pos collide_visit = here;

    bool last_row = false;
    while (true) {
      int tile_x = collide_visit.tile_x();
      int tile_y = collide_visit.tile_y();
      // Out of bounds has no collision
      if (tile_x > 0 && tile_y > 0 && MAX_COLLIDE_Y > tile_y &&
          MAX_COLLIDE_X > tile_x) {
        if (!func(collide_visit)) {
          return;
        }
      }

      if (collide_visit.x == end_x) {
        collide_visit.x = here.x;
        collide_visit.y += TILE_SUBPIXEL_SIZE;

        if (last_row) {
          return;
        }
        if (collide_visit.y >= end_y) {
          collide_visit.y = end_y;
          last_row = true;
        }
      } else {
        collide_visit.x += TILE_SUBPIXEL_SIZE;
        if (collide_visit.x > end_x) {
          collide_visit.x = end_x;
        }
      }
    }
  }
};
