#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/mission_catalog.h"

int main(void) {
  unsigned i, j;
  uint8_t storage[2 + 2 + MISSION_SAVE_BYTES + 2];
  uint8_t *record = storage + 2;
  const uint8_t old_complete[2 + MISSION_SAVE_BYTES] = {
    'M', 'I', 255, 255, 255, 255, 255, 255, 3
  };
  const uint8_t new_complete[2 + MISSION_SAVE_BYTES] = {
    'M', 'I', 255, 255, 255, 255, 255, 255, 15
  };

  /* The actual cartridge catalog must include both authored endgame goals. */
  assert(NMIS == 52);
  assert(!strcmp(MIS[0].name, "FIRST STEPS"));
  assert(!strcmp(MIS[8].name, "SCENT MASTER"));
  assert(!strcmp(MIS[17].name, "CRYSTAL AGE"));
  assert(!strcmp(MIS[26].name, "SKY MOUND"));
  assert(!strcmp(MIS[39].name, "POOR PALACE"));
  assert(!strcmp(MIS[49].name, "FOREIGN FRIENDS"));
  assert(!strcmp(MIS[50].name, "EVERYTHING"));
  assert(!strcmp(MIS[51].name, "GRAND FINALE"));
  assert(MIS[50].kind == MK_VERSE && MIS[50].goal == MG_ALL);
  assert(MIS[51].kind == MK_VERSE && MIS[51].goal == MG_FINAL);
  assert(MIS[51].riv == 3 && MIS[51].w == 96 && MIS[51].h == 96);
  for (i = 0; i < NMIS; i++) {
    assert(MIS[i].name && MIS[i].name[0]);
    assert(MIS[i].kind <= MK_VERSE && MIS[i].goal <= MG_MEGA);
    assert(MIS[i].w >= 32 && MIS[i].w <= 96 && !(MIS[i].w & 7));
    assert(MIS[i].h >= 32 && MIS[i].h <= 96 && !(MIS[i].h & 7));
    for (j = 0; j < 3; j++) assert(MIS[i].s[j] && MIS[i].s[j][0]);
    for (j = 0; j < i; j++) assert(strcmp(MIS[i].name, MIS[j].name));
  }

  /* Load a real old-format 50/50 record, then complete the restored missions.
   * Existing progress must retain its identities and adjacent SRAM bytes. */
  memset(storage, 0xa5, sizeof(storage));
  memcpy(record, old_complete, sizeof(old_complete));
  assert(mission_progress_count(record) == 50);
  assert(!mission_progress_done(record, 50));
  assert(!mission_progress_done(record, 51));
  mission_progress_mark(record, 50);
  assert(mission_progress_count(record) == 51);
  assert(record[8] == 7);
  mission_progress_mark(record, 51);
  assert(mission_progress_count(record) == 52);
  assert(!memcmp(record, new_complete, sizeof(new_complete)));
  mission_progress_mark(record, 51);
  assert(!memcmp(record, new_complete, sizeof(new_complete)));
  mission_progress_mark(record, 52);
  mission_progress_mark(record, 255);
  assert(!mission_progress_done(record, 52));
  assert(!mission_progress_done(record, 255));
  assert(!memcmp(record, new_complete, sizeof(new_complete)));
  assert(storage[0] == 0xa5 && storage[1] == 0xa5);
  assert(storage[sizeof(storage)-1] == 0xa5);
  assert(storage[sizeof(storage)-2] == 0xa5);

  /* A blank cartridge must initialize only its reserved nine-byte record. */
  memset(record, 255, sizeof(new_complete));
  assert(mission_progress_count(record) == 0);
  mission_progress_mark(record, 51);
  assert(record[0] == 'M' && record[1] == 'I');
  assert(record[8] == 8 && mission_progress_count(record) == 1);
  for (i = 0; i < 50; i++) assert(!mission_progress_done(record, (uint8_t)i));
  for (i = 0; i < NMIS; i++) mission_progress_mark(record, (uint8_t)i);
  assert(!memcmp(record, new_complete, sizeof(new_complete)));
  assert(storage[0] == 0xa5 && storage[1] == 0xa5);
  assert(storage[sizeof(storage)-1] == 0xa5);
  assert(storage[sizeof(storage)-2] == 0xa5);
  puts("GBA mission catalog PASS: all 52 missions, both finales, old-save identity and bounded SRAM writes");
  return 0;
}
