# Draws the hand-made maps as 32x32 text: w water, s sand, g grass, h hill, f food patch (grass), Y your nest, R red nest
import math
def new(c='g'): return [[c]*32 for _ in range(32)]
def rect(m,x0,y0,x1,y1,c):
    for y in range(max(0,y0),min(32,y1+1)):
        for x in range(max(0,x0),min(32,x1+1)): m[y][x]=c
def disc(m,cx,cy,r,c):
    for y in range(32):
        for x in range(32):
            if (x-cx)**2+(y-cy)**2<=r*r: m[y][x]=c
def ring(m,cx,cy,r0,r1,c):
    for y in range(32):
        for x in range(32):
            d=(x-cx)**2+(y-cy)**2
            if r0*r0<=d<=r1*r1: m[y][x]=c
maps=[]
# 1 THE MOAT: red queen on a hill fort inside a moat, one sand gate; you start in a quiet meadow
m=new()
disc(m,24,8,8,'s'); ring(m,24,8,6,8,'w'); disc(m,24,8,5,'h'); disc(m,24,8,2,'g')
rect(m,14,7,19,8,'s'); rect(m,20,7,20,8,'g')                    # the gate (west side): sand bridge, then a ramp in
for (x,y) in [(5,8),(10,14),(14,24),(26,25),(8,20),(20,18)]: m[y][x]='f'
disc(m,12,6,3,'h'); disc(m,27,22,3,'h'); disc(m,6,14,2,'w')
m[26][4]='Y'; m[8][24]='R'
maps.append(("THE MOAT","ONE GATE INTO THE RED FORT",m))
# 2 TWIN ISLES: two islands, one thin land bridge in the middle, a berry island between
m=new('w')
disc(m,8,23,10,'s'); disc(m,8,23,8,'g'); disc(m,23,8,10,'s'); disc(m,23,8,8,'g')
disc(m,6,26,2,'h'); disc(m,25,5,2,'h')
for i in range(12,20): m[31-i][i]='s'; m[31-i][i+1]='s'               # narrow diagonal bridge
disc(m,26,26,3,'s'); disc(m,26,26,2,'g'); m[26][26]='f'                # a lonely island: build your own bridge
for (x,y) in [(10,18),(4,20),(21,12),(27,11),(15,16)]: m[y][x]='f'
m[26][4]='Y'; m[5][27]='R'
maps.append(("TWIN ISLES","ONE THIN BRIDGE",m))
# 3 THE MAZE: high walls (hill) between low sand paths, cliffs everywhere, a few ramps of grass
m=new('h')
for y in range(1,31,4): rect(m,1,y,30,y+1,'s')
for x in range(1,31,6): rect(m,x,1,x+1,30,'s')
for (x0,y0,x1,y1) in [(4,3,6,4),(12,7,14,8),(20,11,22,12),(8,15,10,16),(16,19,18,20),(24,23,26,24),(2,27,4,28),(26,3,28,4)]: rect(m,x0,y0,x1,y1,'h')
for (x,y) in [(9,5),(15,13),(21,21),(27,9),(3,17)]: m[y][x]='f'
for (x,y) in [(5,10),(11,22),(23,6),(17,27)]: m[y][x]='g'                   # little ramps up the walls
m[26][4]='Y'; m[5][27]='R'
maps.append(("THE MAZE","STAY ON THE PATHS",m))
import sys
out=open(sys.argv[1] if len(sys.argv)>1 else 'src/maps.h','w')
out.write('''// HAND-MADE MAPS. Each map: name, description, width, height (32..96), then its rows of letters:
//   w water   s sand   g grass   h hill   f grass with a berry patch   Y your nest   R red nest
// Paste new maps from the MAP MAKER app at the end of the list (just before the closing "};") and they appear on the title screen.
#pragma once
typedef struct { const char *name, *desc; u8 w, h; const char *cells; } HandMap;   // cells: w*h letters, row by row
static const HandMap HMAPS[] = {
''')
for n,d,mm in maps:
    out.write('  {"%s", "%s", %d, %d,\n'%(n,d,len(mm[0]),len(mm)))
    for r in mm: out.write('    "%s"\n'%''.join(r))
    out.write('  },\n')
out.write('''};
#define NHAND ((u8)(sizeof(HMAPS) / sizeof(HMAPS[0])))
''')
out.close()
for n,d,mm in maps:
    print(n); print('\n'.join(''.join(r) for r in mm[:6]))
