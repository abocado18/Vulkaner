#pragma once

#include "ecs/vox_ecs.h"



#include "platform/math/math.h"
#include <bits/types/timer_t.h>

#include "plugin.h"


namespace game {

struct Time {
  float delta_time;
  float total_time;
  uint64_t total_ticks;
};


//Global Data for Game
struct GameData {

  bool should_run;

};

class Game {
public:
  Game();
  ~Game();

  void tick();

  void runStartup();

  void addPlugin(IPlugin &p) { p.build(*this); }

  const bool shouldRun();

  vecs::Ecs world;

  vecs::Schedule Startup = {};

  vecs::Schedule PreUpdate = {};
  vecs::Schedule Update = {};
  vecs::Schedule PostUpdate = {};

  vecs::Schedule OnClose = {};

private:
  Time time_data = {};

  std::chrono::time_point<std::chrono::high_resolution_clock> last_frame_start;

 
};

} // namespace game