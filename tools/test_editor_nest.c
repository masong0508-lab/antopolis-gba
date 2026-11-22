/* Compile the actual GBA Map Maker resize helper on the host.
   Cover all 81 supported dimensions, adversarial cropped queens,
   and 40,500 deterministic position combinations. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/editor_nest.h"

static uint32_t rnd_state = 0xC0FFEE25u;
static uint8_t next_u8(void) {
  rnd_state ^= rnd_state << 13;
  rnd_state ^= rnd_state >> 17;
  rnd_state ^= rnd_state << 5;
  return (uint8_t)(rnd_state >> 9);
}

static void check(uint8_t width, uint8_t height,
                  uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
  uint8_t x[2] = {x0,x1}, y[2] = {y0,y1};
  uint8_t want_x0 = x0 >= width ? (uint8_t)(width - 5u) : x0;
  uint8_t want_x1 = x1 >= width ? (uint8_t)(width - 5u) : x1;
  uint8_t want_y0 = y0 >= height ? (uint8_t)(height - 5u) : y0;
  uint8_t want_y1 = y1 >= height ? (uint8_t)(height - 5u) : y1;
  uint8_t collided = (uint8_t)(want_x0 == want_x1 && want_y0 == want_y1);
  uint8_t expected_move = (uint8_t)(x0 != want_x0 || y0 != want_y0 ||
      x1 != want_x1 || y1 != want_y1 || collided);
  uint8_t moved = editor_nests_fit(width,height,x,y);
  assert(moved == expected_move);
  assert(x[0] == want_x0 && y[0] == want_y0);
  assert(x[0] < width && y[0] < height);
  assert(x[1] < width && y[1] < height);
  assert(x[0] != x[1] || y[0] != y[1]);
  if (!collided) {
    /* Legal, distinct player-authored nest positions are sacrosanct. */
    assert(x[1] == want_x1 && y[1] == want_y1);
  } else {
    uint8_t expected_x = want_x1 >= 8u ?
        (uint8_t)(want_x1 - 4u) : (uint8_t)(want_x1 + 4u);
    assert(x[1] == expected_x && y[1] == want_y1);
    assert((x[1] > x[0] ? x[1]-x[0] : x[0]-x[1]) == 4u);
  }
  /* The helper is idempotent: repeated menu redraw or resizing to the
     same dimensions cannot move either colony a second time. */
  assert(editor_nests_fit(width,height,x,y) == 0u);
}

int main(void) {
  uint8_t width,height;
  uint32_t tested=0u;
  static const uint8_t special[] = {0u,1u,4u,7u,8u,15u,31u,
      32u,63u,64u,95u,96u,255u};
  uint8_t i,j;
  for (height=32u; height<=96u; height=(uint8_t)(height+8u))
    for (width=32u; width<=96u; width=(uint8_t)(width+8u)) {
      /* Real default queens remain distinct on every world shape. */
      check(width,height,4u,(uint8_t)(height-6u),
            (uint8_t)(width-5u),5u);
      ++tested;
      /* Force both nests to crop from the same out-of-range corner. */
      check(width,height,96u,96u,127u,127u);
      check(width,height,255u,255u,255u,255u);
      tested += 2u;
      for (i=0u;i<sizeof special;++i)
        for (j=0u;j<sizeof special;++j) {
          check(width,height,special[i],special[j],
                special[j],special[i]);
          ++tested;
        }
      for (i=0;i<244u;++i) {
        check(width,height,next_u8(),next_u8(),next_u8(),next_u8());
        ++tested;
      }
    }
  assert(tested == 81u*(3u + 13u*13u + 244u));
  printf("PASS: %lu Map Maker nest layouts, 81 dimensions, collision repair, idempotence\n",
         (unsigned long)tested);
  return 0;
}
