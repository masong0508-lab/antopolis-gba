#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/mission_catalog.h"

int main(void) {
  unsigned i, j;
  uint8_t storage[2 + 2 + MISSION_SAVE_BYTES + 2];
  uint8_t *record = storage + 2;
  const uint8_t old_fifty[2 + MISSION_SAVE_BYTES] = {'M','I',255,255,255,255,255,255,3};
  const uint8_t old_fifty_two[2 + MISSION_SAVE_BYTES] = {'M','I',255,255,255,255,255,255,15};
  const uint8_t all_fifty_six[2 + MISSION_SAVE_BYTES] = {'M','I',255,255,255,255,255,255,255};

  /* Persistent mission identity is the SRAM bit position: append only. */
  assert(NMIS == 56 && MISSION_SAVE_BYTES == 7);
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
  assert(!strcmp(MIS[52].name, "SCOUTS FIRST"));
  assert(MIS[52].kind == MK_EYE && MIS[52].goal == MG_EFOOD && MIS[52].v1 == 3);
  assert(!strcmp(MIS[53].name, "NEST GUARD"));
  assert(MIS[53].kind == MK_CLASSIC && MIS[53].goal == MG_CFLAW);
  assert(!strcmp(MIS[54].name, "ROOT TOWN"));
  assert(MIS[54].kind == MK_EMPIRE && MIS[54].goal == MG_EBLD && MIS[54].v1 == 12);
  assert(!strcmp(MIS[55].name, "PARKS AND PATHS"));
  assert(MIS[55].kind == MK_CITY && MIS[55].goal == MG_CSMOG);
  for (i = 0; i < NMIS; i++) {
    assert(MIS[i].name && MIS[i].name[0]);
    assert(MIS[i].kind <= MK_VERSE && MIS[i].goal <= MG_MEGA);
    assert(MIS[i].w >= 32 && MIS[i].w <= 96 && !(MIS[i].w & 7));
    assert(MIS[i].h >= 32 && MIS[i].h <= 96 && !(MIS[i].h & 7));
    for (j = 0; j < 3; j++) assert(MIS[i].s[j] && MIS[i].s[j][0]);
    for (j = 0; j < i; j++) assert(strcmp(MIS[i].name, MIS[j].name));
  }

  /* Old 50/50 completion and surrounding saved data must survive. */
  memset(storage, 0xa5, sizeof storage);
  memcpy(record, old_fifty, sizeof old_fifty);
  assert(mission_progress_count(record) == 50);
  for (i = 0; i < 50; i++) assert(mission_progress_done(record, (uint8_t)i));
  for (i = 50; i < NMIS; i++) assert(!mission_progress_done(record, (uint8_t)i));
  mission_progress_mark(record, 50);
  mission_progress_mark(record, 51);
  assert(mission_progress_count(record) == 52);
  assert(!memcmp(record, old_fifty_two, sizeof old_fifty_two));
  for (i = 52; i < NMIS; i++) mission_progress_mark(record, (uint8_t)i);
  assert(mission_progress_count(record) == 56);
  assert(!memcmp(record, all_fifty_six, sizeof all_fifty_six));
  mission_progress_mark(record, 55);
  mission_progress_mark(record, 56);
  mission_progress_mark(record, 255);
  assert(!mission_progress_done(record, 56) && !mission_progress_done(record, 255));
  assert(!memcmp(record, all_fifty_six, sizeof all_fifty_six));
  assert(storage[0] == 0xa5 && storage[1] == 0xa5);
  assert(storage[sizeof(storage)-1] == 0xa5 && storage[sizeof(storage)-2] == 0xa5);

  /* All previously complete 52/52 saves now have four untouched bonus bits. */
  memcpy(record, old_fifty_two, sizeof old_fifty_two);
  assert(mission_progress_count(record) == 52);
  for (i = 52; i < NMIS; i++) assert(!mission_progress_done(record, (uint8_t)i));
  mission_progress_mark(record, 55);
  assert(mission_progress_count(record) == 53);
  assert(record[8] == 143);

  /* Fresh or invalid SRAM initializes only the original nine bytes. */
  memset(record, 255, sizeof all_fifty_six);
  assert(mission_progress_count(record) == 0);
  mission_progress_mark(record, 55);
  assert(record[0] == 'M' && record[1] == 'I');
  assert(record[8] == 128 && mission_progress_count(record) == 1);
  for (i = 0; i < 55; i++) assert(!mission_progress_done(record, (uint8_t)i));
  for (i = 0; i < NMIS; i++) mission_progress_mark(record, (uint8_t)i);
  assert(!memcmp(record, all_fifty_six, sizeof all_fifty_six));
  assert(storage[0] == 0xa5 && storage[1] == 0xa5);
  assert(storage[sizeof(storage)-1] == 0xa5 && storage[sizeof(storage)-2] == 0xa5);
  /* A new campaign always has a next entry; wrap and skip completed bits.
     No mission reordering or SRAM mutation is permitted by navigation. */
  memcpy(record, old_fifty, sizeof old_fifty);
  assert(mission_progress_next_open(record, 0) == 50);
  assert(mission_progress_next_open(record, 50) == 51);
  assert(mission_progress_next_open(record, 55) == 50);
  assert(mission_progress_next_open(record, 49) == 50);
  assert(!memcmp(record, old_fifty, sizeof old_fifty));
  memcpy(record, old_fifty_two, sizeof old_fifty_two);
  assert(mission_progress_next_open(record, 0) == 52);
  assert(mission_progress_next_open(record, 55) == 52);
  memset(record, 0, sizeof old_fifty);
  assert(mission_progress_next_open(record, 0) == 1);
  /* Once the last unfinished bit is cleared, stay on the same mission. */
  memcpy(record, all_fifty_six, sizeof all_fifty_six);
  assert(mission_progress_next_open(record, 0) == 0);
  assert(mission_progress_next_open(record, 55) == 55);
  record[8] &= (uint8_t)~(1u << 4); /* only mission index 52 incomplete */
  assert(mission_progress_next_open(record, 52) == 52);
  assert(mission_progress_next_open(record, 55) == 52);
  assert(mission_progress_next_open(record, 51) == 52);
  assert(storage[0] == 0xa5 && storage[1] == 0xa5);
  assert(storage[sizeof(storage)-1] == 0xa5 && storage[sizeof(storage)-2] == 0xa5);
  /* Every original and appended mission jumps to the next mode's
     first entry; cycling all six modes returns to the same category. */
  for (i = 0; i < NMIS; ++i) {
    uint8_t next = mission_next_mode((uint8_t)i);
    uint8_t kind = (uint8_t)((MIS[i].kind + 1u) % 6u);
    assert(next < NMIS && MIS[next].kind == kind);
    for (j = 0; j < next; ++j) assert(MIS[j].kind != kind);
    for (j = 0; j < 5; ++j) next = mission_next_mode(next);
    assert(MIS[next].kind == MIS[i].kind);
  }
  assert(mission_next_mode(255) == 5);
  assert(mission_next_mode(51) == 0);
  assert(mission_next_mode(52) == 9);
  /* All six mode totals include appended bonus missions, and counts
     never write to battery data, even with an invalid save header. */
  {
    const uint8_t totals[6] = {6, 5, 10, 10, 13, 12};
    const uint8_t old_done[6] = {5, 4, 9, 9, 13, 10};
    uint8_t done, total, before[sizeof storage];
    for (i = 0; i <= MK_VERSE; ++i) {
      memcpy(record, old_fifty, sizeof old_fifty);
      memcpy(before, storage, sizeof storage);
      mission_progress_mode(record, (uint8_t)i, &done, &total);
      assert(total == totals[i] && done == old_done[i]);
      assert(!memcmp(storage, before, sizeof storage));
      memcpy(record, all_fifty_six, sizeof all_fifty_six);
      mission_progress_mode(record, (uint8_t)i, &done, &total);
      assert(total == totals[i] && done == total);
      memset(record, 0xff, sizeof old_fifty);
      mission_progress_mode(record, (uint8_t)i, &done, &total);
      assert(total == totals[i] && done == 0);
    }
    for (i = 0; i < NMIS; ++i) {
      memset(record, 0, sizeof old_fifty);
      mission_progress_mark(record, (uint8_t)i);
      memcpy(before, storage, sizeof storage);
      for (j = 0; j <= MK_VERSE; ++j) {
        mission_progress_mode(record, (uint8_t)j, &done, &total);
        assert(total == totals[j] && done == (MIS[i].kind == j));
      }
      assert(!memcmp(storage, before, sizeof storage));
    }
    mission_progress_mode(record, 255, &done, &total);
    assert(done == 0 && total == 0);
  }
  puts("GBA mission catalog PASS: 56 missions, mode progress, next-open and next-mode navigation, SRAM intact");
  return 0;
}
