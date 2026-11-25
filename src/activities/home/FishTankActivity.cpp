#include "FishTankActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>

#include <cstdio>
#include <cstring>

#include "FishTankStore.h"
#include "MappedInputManager.h"
#include "fontIds.h"

using fishtank::MAX_FISH;

// ---------------------------------------------------------------------------
// Tuning — tweak these freely
// ---------------------------------------------------------------------------
namespace {
constexpr unsigned long TICK_MS = 800;         // one animation frame (e-ink fast refresh)
constexpr uint16_t CLEAN_EVERY = 25;           // half refresh every N frames to clear ghosting
constexpr int64_t FEED_EVERY_S = 6 * 3600;     // one meal every 6 hours
constexpr int64_t HUNGRY_AFTER_S = 12 * 3600;  // fish get sluggish after 12 h without food
constexpr int64_t DAY_S = 24 * 3600;
constexpr int CLEAN_DAYS = 7;                  // tank should be cleaned every 7 days
constexpr int SAND_H = 60;                     // sand height in screen px
constexpr int TOP_H = 70;                      // status bar height
constexpr int MSG_FRAMES = 8;                  // how long a banner message stays
// The tank is always landscape. If it shows up upside-down for the way you hold
// the device, swap to GfxRenderer::Orientation::LandscapeCounterClockwise.
constexpr auto TANK_ORIENTATION = GfxRenderer::Orientation::LandscapeClockwise;

// ---------------------------------------------------------------------------
// Sprites: '#' = black art pixel, anything else = transparent.
// All fish face RIGHT; they are mirrored when swimming left.
// ---------------------------------------------------------------------------
struct Sprite {
  const char* const* rows;
  uint8_t w, h;
};

const char* const ROUND_F0[] = {
    "......#####...", "#...#########.", "##.########.##", ".#############",
    ".#############", "##..#########.", "#.....#####...",
};
const char* const ROUND_F1[] = {
    "......#####...", "....#########.", ".#.########.##", ".#############",
    ".#############", ".#..#########.", "......#####...",
};
const char* const SLIM_F0[] = {
    "#.....#######...", "##..#########.#.", ".###############", "##..##.#.#.####.", "#.....#######...",
};
const char* const SLIM_F1[] = {
    "......#######...", ".#..#########.#.", ".###############", ".#..##.#.#.####.", "......#######...",
};
const char* const ANGEL_F0[] = {
    "......#....", ".....##....", "#...####...", "##.######..", ".#######.##",
    "##.#######.", "#...####...", ".....##....", "......#....",
};
const char* const ANGEL_F1[] = {
    "......#....", ".....##....", ".#..####...", ".#.######..", ".#######.##",
    ".#.#######.", ".#..####...", ".....##....", "......#....",
};
const char* const PUFFER_F0[] = {
    "...#..#..#..", "....######..", "#..########.", "##.#####.###", "############",
    "##.#########", "#..########.", "....######..", "...#..#..#..",
};
const char* const PUFFER_F1[] = {
    "...#..#..#..", "....######..", ".#.########.", ".#.#####.###", "############",
    ".#.#########", ".#.########.", "....######..", "...#..#..#..",
};
const char* const GOLD_F0[] = {
    "###.....####...", ".###..#######..", "..##########.#.", "....##########.",
    "..############.", ".###..#######..", "###.....####...", "........#......",
};
const char* const GOLD_F1[] = {
    ".##.....####...", "..###.#######..", "..##########.#.", "....##########.",
    "..############.", "..###.#######..", ".##.....####...", "........#......",
};
const char* const BETTA_F0[] = {
    "......####....", "#...#######...", "##..########..", "###.#######.##", "##############",
    "###.##########", "##..########..", "#...##.##.#...", "....#..#..#...", ".....#..#.....",
};
const char* const BETTA_F1[] = {
    "......####....", "....#######...", ".#..########..", ".##.#######.##", ".#############",
    ".##.##########", ".#..########..", "....##.##.#...", "....#..#..#...", ".....#..#.....",
};
const char* const SHARK_F0[] = {
    "........#.........", ".......##.........", "#.....##########..", "##.###########.###",
    ".#################", "##.....######.#...", "#.......#.........",
};
const char* const SHARK_F1[] = {
    "........#.........", ".......##.........", ".#....##########..", ".#.###########.###",
    ".#################", ".#.....######.#...", "........#.........",
};
const char* const ANGLER_F0[] = {
    "...........##.", "..........#...", ".....######...", "#..#########..", "##.######.###.",
    ".############.", "##.##.#.#.#.##", "#..##########.", ".....######...",
};
const char* const ANGLER_F1[] = {
    "..........###.", "..........#...", ".....######...", ".#.#########..", ".#.######.###.",
    ".############.", ".#.##.#.#.#.##", ".#.##########.", ".....######...",
};
const char* const CLOWN_F0[] = {
    ".....######...", "#..###.##.##..", "##.###.##.#.##", ".#####.##.####",
    ".#####.##.####", "##.###.##.##..", "#....######...",
};
const char* const CLOWN_F1[] = {
    ".....######...", "...###.##.##..", ".#.###.##.#.##", ".#####.##.####",
    ".#####.##.####", ".#.###.##.##..", ".....######...",
};
// Plants: drawn from the bottom row up; upper rows sway more.
const char* const FERN[] = {
    ".##......", "...##....", "....#....", "....#....", "....#.##.", "....##...", "....#....", "....#....",
    ".##.#....", "...##....", "....#....", "....#....", "....#.##.", "....##...", "....#....", "....#....",
    ".##.#....", "...##....", "....#....", "....#....", "....#.##.", "....##...", "....#....", "....#....",
    ".##.#....", "...##....", "....#....", "....#....", "....#.##.", "....##...", "....#....", "....#....",
    "....#....", "....#....", "....#....", "....#....", "....#....", "....#....",
};
const char* const HEART[] = {".#.#.", "#####", ".###.", "..#.."};
const char* const BUBBLE[] = {".#.", "#.#", ".#."};
const char* const WORM_F0[] = {"##..##", "..##.."};
const char* const WORM_F1[] = {"..##..", "##..##"};

// Order matters: saved fish remember their kind by index, so only append.
const Sprite FISH_KINDS[][2] = {
    {{ROUND_F0, 14, 7}, {ROUND_F1, 14, 7}},     // 0 ปลากลม
    {{SLIM_F0, 16, 5}, {SLIM_F1, 16, 5}},       // 1 ปลายาวลายจุด
    {{ANGEL_F0, 11, 9}, {ANGEL_F1, 11, 9}},     // 2 ปลาเทวดา
    {{PUFFER_F0, 12, 9}, {PUFFER_F1, 12, 9}},   // 3 ปลาปักเป้า
    {{BETTA_F0, 14, 10}, {BETTA_F1, 14, 10}},   // 4 ปลากัด
    {{GOLD_F0, 15, 8}, {GOLD_F1, 15, 8}},       // 5 ปลาทองหางพลิ้ว
    {{SHARK_F0, 18, 7}, {SHARK_F1, 18, 7}},     // 6 ฉลามจิ๋ว
    {{ANGLER_F0, 14, 9}, {ANGLER_F1, 14, 9}},   // 7 ปลาตกเบ็ด
    {{CLOWN_F0, 14, 7}, {CLOWN_F1, 14, 7}},     // 8 ปลาการ์ตูน
};
constexpr int KIND_COUNT = sizeof(FISH_KINDS) / sizeof(FISH_KINDS[0]);
static_assert(KIND_COUNT == fishtank::FISH_KIND_COUNT, "update FISH_KIND_COUNT in FishTankStore.h");

// Murk pattern: 4x4 ordered dither, and how many of the 16 cells are dark
// for each "day since cleaning" (index 0 = day 1 = crystal clear).
const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
const uint8_t MURK_BY_DAY[CLEAN_DAYS] = {0, 1, 2, 3, 5, 7, 9};  // day 7 and later = darkest

// ---------------------------------------------------------------------------
// Runtime world (positions are not saved; growth and names are)
// ---------------------------------------------------------------------------
struct Fish {
  int x, y;  // top-left, screen px
  int8_t dir;
  uint8_t speed;
  uint8_t happy;  // frames left to show a heart
};
struct Dot {
  int x, y;
  bool alive;
  bool worm;  // bookworm treat instead of a regular pellet
};

constexpr int FOOD_N = 10;
constexpr int BUBBLE_N = 8;

Fish fish[MAX_FISH];
int fishSpawned = 0;
Dot food[FOOD_N];
Dot bubbles[BUBBLE_N];
uint32_t frame = 0;
char banner[48] = "";
uint8_t bannerFrames = 0;
uint32_t rng = 12345;

int rnd(int lo, int hi) {  // inclusive range, tiny LCG
  rng = rng * 1664525u + 1013904223u;
  return lo + static_cast<int>((rng >> 8) % static_cast<uint32_t>(hi - lo + 1));
}

int fishPx(int i) {  // art-pixel size grows with bookworms eaten: 3 (baby) .. 7
  const int px = 3 + fishtank::state().fish[i].grown / 2;
  return px > 7 ? 7 : px;
}

int64_t secsUntilMeal() {
  const int64_t left = fishtank::state().lastFed + FEED_EVERY_S - fishtank::nowSec();
  return left > 0 ? left : 0;
}
int daysSinceClean() { return static_cast<int>((fishtank::nowSec() - fishtank::state().lastCleaned) / DAY_S) + 1; }
bool fishHungry() { return fishtank::nowSec() - fishtank::state().lastFed >= HUNGRY_AFTER_S; }

void showBanner(const char* text) {
  strncpy(banner, text, sizeof(banner) - 1);
  banner[sizeof(banner) - 1] = '\0';
  bannerFrames = MSG_FRAMES;
}

// Draw a sprite; with halo, first paint a white outline so it reads on murky water.
void drawSprite(const GfxRenderer& r, const Sprite& s, int x, int y, int px, bool mirror, bool halo) {
  for (int pass = halo ? 0 : 1; pass < 2; ++pass) {
    for (int row = 0; row < s.h; ++row) {
      for (int col = 0; col < s.w; ++col) {
        if (s.rows[row][col] != '#') continue;
        const int c = mirror ? (s.w - 1 - col) : col;
        if (pass == 0)
          r.fillRect(x + c * px - 3, y + row * px - 3, px + 6, px + 6, false);
        else
          r.fillRect(x + c * px, y + row * px, px, px, true);
      }
    }
  }
}

bool dropFood(int x, int y, int count, bool worm) {
  int dropped = 0;
  for (auto& f : food) {
    if (f.alive) continue;
    f = {x + rnd(-30, 30), y + rnd(0, 20), true, worm};
    if (++dropped == count) break;
  }
  return dropped > 0;
}
}  // namespace

// ---------------------------------------------------------------------------

void FishTankActivity::onEnter() {
  Activity::onEnter();
  previousOrientation = renderer.getOrientation();
  renderer.setOrientation(TANK_ORIENTATION);
  const int W = renderer.getScreenWidth();
  const int H = renderer.getScreenHeight();
  auto& s = fishtank::state();
  rng ^= static_cast<uint32_t>(millis());

  // Spawn any fish we don't have on screen yet (first visit, or a new book fish)
  for (; fishSpawned < s.fishCount; ++fishSpawned) {
    const bool newcomer = fishSpawned >= fishtank::STARTER_FISH && s.newFishPending;
    fish[fishSpawned] = {rnd(20, W - 100), newcomer ? TOP_H + 10 : rnd(TOP_H + 20, H - SAND_H - 60),
                         static_cast<int8_t>(rnd(0, 1) ? 1 : -1), static_cast<uint8_t>(rnd(6, 12)),
                         static_cast<uint8_t>(newcomer ? 6 : 0)};
  }
  if (s.newFishPending) {
    char msg[48];
    snprintf(msg, sizeof(msg), "New fish: %s!", s.fish[s.newFishPending - 1].name);
    showBanner(msg);
    s.newFishPending = 0;
    fishtank::save();
  }

  lastTick = millis();
  framesSinceClean = CLEAN_EVERY;  // start with a clean half refresh
  requestUpdate();
}

void FishTankActivity::onExit() {
  renderer.setOrientation(previousOrientation);  // hand Home back its own layout
  Activity::onExit();
}

bool FishTankActivity::feed(int x, int y) {
  if (secsUntilMeal() > 0) {
    showBanner("Not meal time yet");
    return false;
  }
  dropFood(x, y, 3, false);
  fishtank::state().lastFed = fishtank::nowSec();
  fishtank::save();
  return true;
}

bool FishTankActivity::giveWorm(int x, int y) {
  auto& s = fishtank::state();
  if (s.worms == 0) {
    showBanner("Read 20 pages for a bookworm");
    return false;
  }
  if (!dropFood(x, y, 1, true)) return false;
  --s.worms;
  fishtank::save();
  return true;
}

void FishTankActivity::cleanTank() {
  const int W = renderer.getScreenWidth();
  const int H = renderer.getScreenHeight();
  fishtank::state().lastCleaned = fishtank::nowSec();
  fishtank::save();
  for (auto& f : food)
    if (!f.worm) f.alive = false;  // leftovers get scooped out (worms are too precious)
  for (auto& b : bubbles) b = {rnd(30, W - 30), rnd(TOP_H + 40, H - SAND_H - 20), true, false};
  showBanner("Sparkling clean!");
  framesSinceClean = CLEAN_EVERY;  // wipe ghosting along with the dirt
}

void FishTankActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) || mappedInput.wasBackGesture()) {
    finish();
    return;
  }

  using B = MappedInputManager::Button;
  const int W = renderer.getScreenWidth();
  const int H = renderer.getScreenHeight();
  int tx = 0, ty = 0;
  bool acted = true;
  {
    RenderLock lock(*this);
    if (mappedInput.wasReleased(B::Confirm)) {
      feed(rnd(60, W - 60), TOP_H + 10);
    } else if (mappedInput.wasReleased(B::Up) || mappedInput.wasReleased(B::Left)) {
      giveWorm(rnd(60, W - 60), TOP_H + 10);
    } else if (mappedInput.wasReleased(B::Down) || mappedInput.wasReleased(B::Right)) {
      cleanTank();
    } else if (mappedInput.wasScreenTapped(tx, ty)) {
      if (ty < TOP_H) {  // top bar = three touch buttons: FEED | WORM | CLEAN
        if (tx < W / 3)
          feed(rnd(60, W - 60), TOP_H + 10);
        else if (tx < 2 * W / 3)
          giveWorm(rnd(60, W - 60), TOP_H + 10);
        else
          cleanTank();
      } else if (ty < H - SAND_H) {
        feed(tx, ty);  // tap the water = meal at that spot
      }
    } else {
      acted = false;
    }
  }

  const unsigned long now = millis();
  if (acted || now - lastTick >= TICK_MS) {
    lastTick = now;
    {
      RenderLock lock(*this);
      step();
    }
    requestUpdate();
  }
}

void FishTankActivity::step() {
  const int W = renderer.getScreenWidth();
  const int H = renderer.getScreenHeight();
  const int floorY = H - SAND_H;
  auto& s = fishtank::state();
  ++frame;
  if (bannerFrames) --bannerFrames;

  // Food sinks to the sand (worms wriggle down more slowly)
  for (auto& f : food) {
    if (!f.alive) continue;
    if (f.y < floorY - 12) f.y += f.worm ? 6 : 8;
    if (f.worm) f.x += rnd(-2, 2);
  }

  const bool hungry = fishHungry();
  for (int i = 0; i < fishSpawned; ++i) {
    Fish& fi = fish[i];
    const int px = fishPx(i);
    const Sprite& sp = FISH_KINDS[s.fish[i].kind % KIND_COUNT][0];
    const int fw = sp.w * px, fh = sp.h * px;
    int spd = fi.speed;
    if (hungry) spd = spd / 2 + 1;  // hungry fish are sluggish

    // Chase the nearest morsel (worms count as much closer — irresistible)
    Dot* target = nullptr;
    long best = 0;
    const int mouthX = fi.x + (fi.dir > 0 ? fw : 0);
    const int mouthY = fi.y + fh / 2;
    for (auto& f : food) {
      if (!f.alive) continue;
      const long dx = f.x - mouthX, dy = f.y - mouthY;
      long d = dx * dx + dy * dy;
      if (f.worm) d /= 4;
      if (!target || d < best) {
        target = &f;
        best = d;
      }
    }

    if (target) {
      spd += 6;  // excited!
      const long dx = target->x - mouthX, dy = target->y - mouthY;
      if (dx * dx + dy * dy < 22L * 22L) {
        target->alive = false;
        fi.happy = target->worm ? 8 : 4;
        if (target->worm && s.fish[i].grown < fishtank::MAX_GROWN) {
          ++s.fish[i].grown;  // bookworms make fish grow
          fishtank::save();
        }
        continue;
      }
      if (dx > 4) fi.dir = 1;
      if (dx < -4) fi.dir = -1;
      fi.x += (abs(dx) < spd) ? dx : fi.dir * spd;
      fi.y += (abs(dy) < spd) ? dy : (dy > 0 ? spd : -spd);
    } else {
      fi.x += fi.dir * spd;  // lazy wander
      if (rnd(0, 9) == 0) fi.dir = -fi.dir;
      fi.y += rnd(-4, 4);
    }

    // Stay inside the tank
    if (fi.x < 10) { fi.x = 10; fi.dir = 1; }
    if (fi.x > W - fw - 10) { fi.x = W - fw - 10; fi.dir = -1; }
    if (fi.y < TOP_H + 10) fi.y = TOP_H + 10;
    if (fi.y > floorY - fh - 24) fi.y = floorY - fh - 24;  // room for a name tag
    if (fi.happy) --fi.happy;
  }

  // Bubbles: spawn from the left rock, rise, pop at the top
  bool spawned = false;
  for (auto& b : bubbles) {
    if (b.alive) {
      b.y -= 18;
      b.x += rnd(-3, 3);
      if (b.y < TOP_H) b.alive = false;
    } else if (!spawned && rnd(0, 6) == 0) {
      b = {70 + rnd(-6, 6), floorY - 30, true, false};
      spawned = true;
    }
  }
}

void FishTankActivity::render(RenderLock&&) {
  const int W = renderer.getScreenWidth();
  const int H = renderer.getScreenHeight();
  const int floorY = H - SAND_H;
  const auto& s = fishtank::state();
  const int day = daysSinceClean();
  renderer.clearScreen();

  // --- Murky water (drawn first so everything else sits on top) ---
  const int murk = MURK_BY_DAY[(day < CLEAN_DAYS ? day : CLEAN_DAYS) - 1];
  const bool halo = murk > 0;
  if (murk > 0) {
    for (int y = TOP_H + 2; y < floorY; y += 3) {
      for (int x = 0; x < W; x += 3) {
        if (BAYER[(y / 3) & 3][(x / 3) & 3] < murk) renderer.fillRect(x, y, 2, 2, true);
      }
    }
  }

  // --- Status bar: three touch buttons (FEED | WORM | CLEAN) ---
  const int col = W / 3;
  char line[32];
  const int64_t left = secsUntilMeal();
  renderer.drawText(UI_12_FONT_ID, 14, 10, "FEED");
  if (left == 0)
    snprintf(line, sizeof(line), "ready! (OK)");
  else
    snprintf(line, sizeof(line), "in %dh%02dm", static_cast<int>(left / 3600), static_cast<int>((left % 3600) / 60));
  renderer.drawText(SMALL_FONT_ID, 14, 40, line);

  snprintf(line, sizeof(line), "BOOKWORM x%d", s.worms);
  renderer.drawText(UI_12_FONT_ID, col + 14, 10, line);
  snprintf(line, sizeof(line), "%d/%d pages", s.pagesTowardWorm, fishtank::PAGES_PER_WORM);
  renderer.drawText(SMALL_FONT_ID, col + 14, 40, line);

  renderer.drawText(UI_12_FONT_ID, 2 * col + 14, 10, "WATER");
  if (day >= CLEAN_DAYS)
    snprintf(line, sizeof(line), "clean now!");
  else
    snprintf(line, sizeof(line), "day %d/%d", day, CLEAN_DAYS);
  renderer.drawText(SMALL_FONT_ID, 2 * col + 14, 40, line);

  renderer.drawLine(col, 6, col, TOP_H - 6, 2, true);
  renderer.drawLine(2 * col, 6, 2 * col, TOP_H - 6, 2, true);
  renderer.drawLine(0, TOP_H, W, TOP_H, 2, true);

  // --- Sand ---
  renderer.drawLine(0, floorY, W, floorY, 2, true);
  for (int y = floorY + 8; y < H; y += 10) {
    for (int x = (y / 10) % 2 ? 6 : 0; x < W; x += 14) renderer.fillRect(x, y, 3, 3, true);
  }

  // Rocks
  renderer.fillRoundedRect(40, floorY - 30, 70, 34, 14, Color::Black);
  renderer.fillRoundedRect(W - 150, floorY - 22, 50, 26, 10, Color::Black);

  // Plants (sway more toward the top)
  static const int8_t sway[] = {0, 1, 2, 2, 1, 0, -1, -2, -2, -1};
  // 1) Straight seaweed strands
  const int maxSeg = (floorY - TOP_H) * 2 / 3 / 12;  // reaches ~2/3 of the water
  const int weedX[] = {W / 2 - 40, W / 2 - 10, W - 60};
  const int weedH[] = {maxSeg, maxSeg * 2 / 3, maxSeg * 3 / 4};
  for (int w = 0; w < 3; ++w) {
    for (int seg = 0; seg < weedH[w]; ++seg) {
      const int off = sway[(seg + frame + w * 3) % 10] * (seg / 4);
      renderer.fillRect(weedX[w] + off * 2, floorY - (seg + 1) * 12, 8, 12, true);
    }
  }
  // 2) Branching ferns with alternating twigs
  struct Plant {
    const char* const* rows;
    uint8_t w, h, px;
    int x;  // left edge, screen px
  };
  const Plant plants[] = {
      {FERN, 9, 38, 3, 140},
      {FERN, 9, 38, 3, W - 210},
  };
  for (int p = 0; p < static_cast<int>(sizeof(plants) / sizeof(plants[0])); ++p) {
    const Plant& pl = plants[p];
    const int top = floorY - pl.h * pl.px;
    for (int row = 0; row < pl.h; ++row) {
      const int lift = pl.h - row;  // rows above the root
      const int off = sway[(frame + row / 6 + p * 4) % 10] * lift * 3 / (pl.h * 2);
      for (int col = 0; col < pl.w; ++col) {
        if (pl.rows[row][col] != '#') continue;
        renderer.fillRect(pl.x + (col + off) * pl.px, top + row * pl.px, pl.px, pl.px, true);
      }
    }
  }

  // Bubbles
  const Sprite bubble{BUBBLE, 3, 3};
  for (const auto& b : bubbles)
    if (b.alive) drawSprite(renderer, bubble, b.x, b.y, 5, false, halo);

  // Food: pellets and wriggling bookworms
  const Sprite worm{(frame % 2) ? WORM_F1 : WORM_F0, 6, 2};
  for (const auto& f : food) {
    if (!f.alive) continue;
    if (f.worm) {
      drawSprite(renderer, worm, f.x, f.y, 4, false, true);
    } else {
      if (halo) renderer.fillRect(f.x - 3, f.y - 3, 14, 14, false);
      renderer.fillRect(f.x, f.y, 8, 8, true);
    }
  }

  // Fish (+ name tags for book fish)
  const Sprite heart{HEART, 5, 4};
  for (int i = 0; i < fishSpawned; ++i) {
    const Fish& fi = fish[i];
    const int px = fishPx(i);
    const Sprite& sp = FISH_KINDS[s.fish[i].kind % KIND_COUNT][(frame + i) % 2];
    drawSprite(renderer, sp, fi.x, fi.y, px, fi.dir < 0, halo);
    if (fi.happy) drawSprite(renderer, heart, fi.x + sp.w * px / 2 - 12, fi.y - 30, 5, false, halo);
    if (s.fish[i].name[0]) {
      char tag[14];
      strncpy(tag, s.fish[i].name, sizeof(tag) - 1);
      tag[sizeof(tag) - 1] = '\0';
      const int tw = renderer.getTextWidth(SMALL_FONT_ID, tag);
      const int tx = fi.x + sp.w * px / 2 - tw / 2;
      const int ty = fi.y + sp.h * px + 4;
      renderer.fillRect(tx - 3, ty - 1, tw + 6, 20, false);
      renderer.drawText(SMALL_FONT_ID, tx, ty, tag);
    }
  }

  // Banner message (events first, then ongoing conditions)
  const char* msg = nullptr;
  if (bannerFrames) msg = banner;
  else if (day > CLEAN_DAYS) msg = "Please clean the tank...";
  else if (fishHungry()) msg = "The fish are hungry...";
  if (msg) {
    const int tw = renderer.getTextWidth(UI_12_FONT_ID, msg);
    renderer.fillRect((W - tw) / 2 - 10, TOP_H + 10, tw + 20, 36, false);
    renderer.drawRect((W - tw) / 2 - 10, TOP_H + 10, tw + 20, 36, 2, true);
    renderer.drawText(UI_12_FONT_ID, (W - tw) / 2, TOP_H + 16, msg);
  }

  if (++framesSinceClean >= CLEAN_EVERY) {
    framesSinceClean = 0;
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  } else {
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  }
}
