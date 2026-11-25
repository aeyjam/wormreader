#pragma once
#include <cstdint>

#include <GfxRenderer.h>

#include "activities/Activity.h"

// A tiny 1-bit pixel aquarium. Fish wander, seaweed sways, bubbles rise.
// One meal every 6 h (Confirm / tap). Water gets murkier each day until you
// clean the tank (Down / tap the right third of the top bar); clean weekly.
// Reading earns bookworm treats (Up) that make fish grow; finishing a book adds a fish.
// Open from Home: long-press Back.
class FishTankActivity final : public Activity {
 public:
  explicit FishTankActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("FishTank", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  void step();
  bool feed(int x, int y);  // false while the 6-hour meal timer is still running
  bool giveWorm(int x, int y);  // bookworm treat, earned by reading
  void cleanTank();

  GfxRenderer::Orientation previousOrientation = GfxRenderer::Orientation::Portrait;
  unsigned long lastTick = 0;
  uint16_t framesSinceClean = 0;
};
