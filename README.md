# mybjev

my attempt to calculate exact expected value of blackjack hands using recursive method

# compile

```bash
gcc -O2 -fPIC -o mybjev.exe ./main.c
```

# how to use

```bash
./mybjev.exe <playerhand> <dealercard> <shoe_composition[10]>
```

**card value count from 0**

A -> 0

TJQK -> 9

for example: player hand 55 vs dealer 6 (single deck)

```
./mybjev.exe 44 5 4 4 4 4 2 3 4 4 4 16
```

output:

```
SURRENDER: -0.50000000
STAND: -0.11166941
HIT: 0.36182255
DOUBLE: 0.72364509
SPLIT: 0.20028260
```

another example: playerhand T6 vs dealer 9 (8 decks)

```
./mybjev.exe 95 8 32 32 32 32 32 32 32 32 32 128
```

output

```
SURRENDER: -0.50000000
STAND: -0.54247350
HIT: -0.50916648
DOUBLE: -1.01833296
```
