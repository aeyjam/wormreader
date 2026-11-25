#pragma once
#include <cstdint>
#include <string>

// Persistent fish-tank state, shared by the tank screen and the reader hooks.
// Stored on the SD card as one small binary file (~400 bytes).
namespace fishtank {

constexpr int STARTER_FISH = 2;
constexpr int FISH_KIND_COUNT = 9;     // must match FISH_KINDS in FishTankActivity.cpp
constexpr int MAX_FISH = 12;          // 2 starters + up to 10 "book fish"
constexpr int PAGES_PER_WORM = 20;    // read this many pages -> 1 bookworm treat
constexpr int MAX_WORMS = 99;
constexpr int MAX_GROWN = 8;          // bookworms a fish can eat before it stops growing
constexpr int FINISHED_MEMORY = 32;   // remembers this many finished books (no duplicate fish)

struct SavedFish {
  uint8_t kind;   // sprite kind
  uint8_t grown;  // bookworms eaten
  char name[24];  // empty for starter fish, else the book's title (ASCII only)
};

struct State {
  uint32_t magic;
  int64_t lastFed;      // epoch seconds
  int64_t lastCleaned;  // epoch seconds
  uint32_t totalPages;
  uint16_t worms;
  uint8_t pagesTowardWorm;
  uint8_t fishCount;
  uint8_t finishedCount;  // ring-buffer fill
  uint8_t finishedNext;   // ring-buffer write index
  uint8_t newFishPending; // index+1 of a fish added since the tank was last opened (0 = none)
  uint8_t reserved;
  uint32_t finished[FINISHED_MEMORY];  // title hashes of finished books
  SavedFish fish[MAX_FISH];
};

State& state();  // loads from SD on first use
void save();
int64_t nowSec();

// Reader hooks
void notePageRead();                             // one forward page turn
void noteBookFinished(const std::string& title);  // reached the end of a book

}  // namespace fishtank
