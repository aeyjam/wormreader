#include "FishTankStore.h"

#include <HalStorage.h>
#include <TrustedTime.h>

#include <cstdio>
#include <cstring>
#include <ctime>

namespace fishtank {
namespace {
constexpr const char* SAVE_PATH = "/.crosspoint/fishtank.bin";
constexpr uint32_t MAGIC = 0x46495333;  // "FIS3"
constexpr int64_t FEED_EVERY_S = 6 * 3600;

State st{};
bool loaded = false;

uint32_t hashTitle(const std::string& s) {  // FNV-1a
  uint32_t h = 2166136261u;
  for (unsigned char c : s) h = (h ^ c) * 16777619u;
  return h ? h : 1;
}

// UI fonts have no Thai glyphs, so a non-ASCII title becomes "Book #n".
void makeName(char* out, size_t n, const std::string& title, int bookNo) {
  bool ascii = !title.empty();
  for (unsigned char c : title)
    if (c >= 0x80) ascii = false;
  if (ascii) {
    strncpy(out, title.c_str(), n - 1);
    out[n - 1] = '\0';
  } else {
    snprintf(out, n, "Book #%d", bookNo);
  }
}
}  // namespace

int64_t nowSec() {
  const int64_t t = trustedtime::trustedNow();
  return t ? t : static_cast<int64_t>(time(nullptr));
}

void save() {
  Storage.mkdir("/.crosspoint");
  HalFile f;
  if (!Storage.openFileForWrite("FISH", SAVE_PATH, f)) return;
  f.write(&st, sizeof(st));
  f.close();
}

State& state() {
  if (loaded) return st;
  loaded = true;
  const int64_t now = nowSec();
  HalFile f;
  bool ok = false;
  if (Storage.exists(SAVE_PATH) && Storage.openFileForRead("FISH", SAVE_PATH, f)) {
    ok = f.read(&st, sizeof(st)) == static_cast<int>(sizeof(st)) && st.magic == MAGIC;
    f.close();
  }
  if (!ok) {  // brand-new tank: clean water, first meal ready, four starter fish
    memset(&st, 0, sizeof(st));
    st.magic = MAGIC;
    st.lastFed = now - FEED_EVERY_S;
    st.lastCleaned = now;
    st.fishCount = STARTER_FISH;
    for (int i = 0; i < STARTER_FISH; ++i) st.fish[i] = {static_cast<uint8_t>(i % FISH_KIND_COUNT), 3, ""};
    save();
  }
  // Tanks made before there were 9 kinds had starters 0,1,0,1: give the last two new looks.
  for (int i = 2; i < STARTER_FISH && i < st.fishCount; ++i) {
    if (st.fish[i].name[0] == '\0' && st.fish[i].kind == i % 2) {
      st.fish[i].kind = static_cast<uint8_t>(i);
      save();
    }
  }
  // If the clock went backwards (lost time), don't let timers run negative.
  if (st.lastFed > now) st.lastFed = now - FEED_EVERY_S;
  if (st.lastCleaned > now) st.lastCleaned = now;
  return st;
}

void notePageRead() {
  State& s = state();
  ++s.totalPages;
  if (++s.pagesTowardWorm >= PAGES_PER_WORM) {
    s.pagesTowardWorm = 0;
    if (s.worms < MAX_WORMS) ++s.worms;
    save();  // write once per worm, not on every page
  }
}

void noteBookFinished(const std::string& title) {
  static uint32_t lastSeen = 0;  // cheap guard: the reader may report the end many times
  const uint32_t h = hashTitle(title);
  if (h == lastSeen) return;
  lastSeen = h;

  State& s = state();
  for (int i = 0; i < s.finishedCount; ++i)
    if (s.finished[i] == h) return;  // already got a fish for this book

  s.finished[s.finishedNext] = h;
  s.finishedNext = (s.finishedNext + 1) % FINISHED_MEMORY;
  if (s.finishedCount < FINISHED_MEMORY) ++s.finishedCount;

  if (s.fishCount < MAX_FISH) {
    SavedFish& f = s.fish[s.fishCount];
    f.kind = static_cast<uint8_t>(h % FISH_KIND_COUNT);
    f.grown = 0;  // book fish arrive as babies
    makeName(f.name, sizeof(f.name), title, s.fishCount - STARTER_FISH + 1);
    ++s.fishCount;
    s.newFishPending = s.fishCount;
  }
  save();
}

}  // namespace fishtank
