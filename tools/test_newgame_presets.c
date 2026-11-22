/* Test both built-in GBA new-game presets as actual production data. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/newgame_presets.h"
int main(void) {
  AntStartPreset easy = ant_newgame_preset(0);
  AntStartPreset wild = ant_newgame_preset(1);
  assert(easy.difficulty == 0 && easy.map == 0);
  assert(easy.width == 48 && easy.height == 48);
  assert(easy.lessons == 1 && easy.sandbox == 0);
  assert(wild.difficulty == 2 && wild.map == 3);
  assert(wild.width == 64 && wild.height == 64);
  assert(wild.lessons == 0 && wild.sandbox == 0);
  for(uint8_t i=0;i<255;i++){
    AntStartPreset v = ant_newgame_preset(i);
    assert(v.width >= 32 && v.width <= 96 && (v.width % 8) == 0);
    assert(v.height >= 32 && v.height <= 96 && (v.height % 8) == 0);
    assert(v.difficulty < 3 && v.map < 4);
  }
  puts("PASS: guided and wild GBA starts are legal and repeatable");
  return 0;
}
