#!/usr/bin/env python3
"""Guard the actual Empire auto-build integration and rendering budget."""
from pathlib import Path

source = Path("src/empire.c").read_text()
makefile = Path("Makefile").read_text()
segment = source.split("// SimCity zones: scan a bounded,", 1)[1].split(
    "// healers bring wounded back", 1
)[0]
assert '#include "empire_zone_scan.h"' in source
assert 'ezscan_x = nestx[0]; ezscan_y = nesty[0];' in source
assert 'empire_zone_next(&ezscan_x, &ezscan_y, W, H);' in segment
assert 'tries < 300' in segment, "Preserve the economy-cycle 300-cell budget"
assert 'nbl < EBN' in segment, "No phantom construction at building capacity"
assert 'if (!add_b(x, y, t)) break;' in segment
assert segment.index('if (!add_b(x, y, t)) break;') < segment.index('rf -= BD[t].f / 2;')
assert 'rnd() % W' not in segment and 'rnd() % H' not in segment
assert segment.count('terr_calc();') == 1
assert 'if (built) terr_calc();' in segment
assert 'eredraw(x, y, 1)' not in segment
assert 'fx_add(x, y, FX_PLUS); built++; ezbuilt++;' in segment
assert 'src/empire_zone_scan.h' in makefile
assert './build/test_empire_zone_scan' in makefile
print("GBA zones: bounded scan, no phantom expense and one territory redraw per build batch")
