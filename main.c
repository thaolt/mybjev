#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>


enum {
    SURRENDER,
    STAND,
    HIT,
    DOUBLE_DOWN,
    SPLIT
};

typedef struct {
    int cards[10];
    int left;
} shoe_t;


typedef struct {
    int cards[21];
    int length;
} hand_t;


typedef struct {
    hand_t dealerHand;
    hand_t playerHands[4];
    int playerSplits;
} game_t;

typedef struct {
    bool das;
    bool rsa;
    bool hsa;
    bool h17;
    bool bjp;
    int max_splits;
} rules_t;

//dealerCacheIndex
//dealerCacheData

typedef struct {
    char **indexes;
    float **dprob;
    size_t len;
} dealerCache_t;

dealerCache_t dealerCache;

typedef struct {
    char **indexes;
    float *ev;
    size_t len;
} playerCache_t;

playerCache_t playerCache;

void dealerPlay(shoe_t shoe, hand_t hand, float* prob, float inputProb);
void _dealerPlay(shoe_t shoe, hand_t hand, float* prob, float inputProb);
void playerPlay(shoe_t shoe, game_t game, float *ev);
float playerHit(shoe_t shoe, hand_t playerHand, hand_t dealerHand);
float playerStand(shoe_t shoe, hand_t playerHand, hand_t dealerHand);
float playerDouble(shoe_t shoe, hand_t playerHand, hand_t dealerHand);
float playerSplit(shoe_t shoe, hand_t hand, hand_t dealerHand, int nsplits);
int handValue(hand_t h);

void initCache()
{

}

void dealerCacheAdd(char * key, float *prob)
{
    if (dealerCache.len == 0) {
        dealerCache.indexes = calloc(1, sizeof(char*));
        dealerCache.dprob = calloc(1, sizeof(float*));
    } else {
        char **nindexes;
        nindexes = reallocarray(dealerCache.indexes, dealerCache.len + 1, sizeof(char*));
//        if (nindexes != dealerCache.indexes) free(dealerCache.indexes);
        dealerCache.indexes = nindexes;
        float **ndprob;
        ndprob = reallocarray(dealerCache.dprob, dealerCache.len + 1, sizeof(float*));
//        if (ndprob != dealerCache.dprob) free(dealerCache.dprob);
        dealerCache.dprob = ndprob;

    }

    dealerCache.indexes[dealerCache.len] = calloc(11, sizeof(char));
    memcpy(dealerCache.indexes[dealerCache.len], key, 11 * sizeof(char));
    dealerCache.dprob[dealerCache.len] = calloc(10, sizeof(float));
    memcpy(dealerCache.dprob[dealerCache.len], prob, 10 * sizeof(float));

    dealerCache.len++;
}

int dealerCacheFind(char* key) {
    for (size_t i = 0; i < dealerCache.len; ++i) {
        if (strcmp(key, dealerCache.indexes[i]) == 0) {
            return i;
        }
    }
    return -1;
}

void destroyCache()
{
}

void hashShoe(shoe_t shoe, char *hash) {
    for (int i = 0; i < 10; ++i) {
        hash[i] = shoe.cards[i]+1;
    }
    hash[10] = 0;
}

void hashPlayerActionEV(int action, shoe_t shoe, hand_t playerHand, hand_t dealerHand, char *hash)
{
    int phv = handValue(playerHand);
    hash[0] = action + 1;
    for (int i = 0; i < 10; ++i) {
        hash[i+1] = shoe.cards[i]+1;
    }
    hash[11] = phv / 100;
    hash[12] = (phv % 100) + 1;
    hash[13] = dealerHand.cards[0] + 1;
    hash[14] = 0;
}

int sumia(int *a, int l) {
    int r = 0;
    for (int i = 0; i < l; ++i) {
        r += a[i];
    }
    return r;
}



void playerCacheAdd(char * key, float ev)
{
    if (playerCache.len == 0) {
        playerCache.indexes = calloc(1, sizeof(char*));
        playerCache.ev = calloc(1, sizeof(float));
    } else {
        char **nindexes;
        nindexes = reallocarray(playerCache.indexes, playerCache.len + 1, sizeof(char*));
        playerCache.indexes = nindexes;
        float *nev;
        nev = reallocarray(playerCache.ev, playerCache.len + 1, sizeof(float));
        playerCache.ev = nev;
    }

    playerCache.indexes[playerCache.len] = calloc(15, sizeof(char));
    memcpy(playerCache.indexes[playerCache.len], key, 15 * sizeof(char));
    playerCache.ev[playerCache.len] = ev;

    playerCache.len++;
}

int playerCacheFind(char* key) {
    for (size_t i = 0; i < playerCache.len; ++i) {
        if (strcmp(key, playerCache.indexes[i]) == 0) {
            return i;
        }
    }
    return -1;
}

float sumfa(float *a, int l) {
    float r = 0;
    for (int i = 0; i < l; ++i) {
        r += a[i];
    }
    return r;
}

int handValue(hand_t h) {
    int na = 0; // number of aces
    int hv = 0;
    int sv = 0;
    for (int i = 0; i < h.length; ++i) {
        hv += h.cards[i] + 1;
        if (h.cards[i] == 0) {
            na++;
        }
    }
    if (na > 0) {
        sv = hv;
        if (sv < 12) sv += 10;
    }
    return hv*100 + sv;
}

int min(int a, int b) {
    return a>b?b:a;
}

int max(int a, int b) {
    return a>b?a:b;
}

float maxf(float a, float b) {
    return a>b?a:b;
}

float favg(float *a, int l)
{
    float r = (float) a[0];
    for (int i = 1; i < l; ++i) {
        r += a[i];
        r /= 2;
    }
    return r;
}

void handAddCard(hand_t *h, int c) {
    h->cards[h->length] = c;
    h->length++;
}

void shoeRmCard(shoe_t *shoe, int c) {
    shoe->cards[c]--;
    shoe->left--;
}

void handDrawCard(hand_t *h, shoe_t *shoe, int c) {
    h->cards[h->length] = c;
    h->length++;
    shoe->cards[c]--;
    shoe->left--;
}

void _dealerPlay(shoe_t shoe, hand_t hand, float* prob, float inputProb)
{
    int dhv = handValue(hand);
    if (dhv / 100 >= 17 || dhv % 100 >= 17) {
        prob[ max(dhv % 100, dhv / 100)-17 ] = 1;
        return;
    }

    for (int c = 0; c < 10; ++c) {
        if (shoe.cards[c] > 0) {
            shoe_t sh = shoe; // clone
            hand_t h = hand;
            float cp = (float)sh.cards[c]/sh.left; // card probability
            handDrawCard(&h, &sh, c);
            int ndhv = handValue(h);
            int hv = ndhv / 100;
            int sv = ndhv % 100;
            if (sv >= 17 || hv >= 17) {
                if (inputProb == 0)
                    prob[ max(hv,sv)-17 ] += cp;
                else
                    prob[ max(hv,sv)-17 ] += inputProb * cp;
            } else {
                if (inputProb == 0)
                    _dealerPlay(sh, h, prob, cp);
                else
                    _dealerPlay(sh, h, prob, inputProb * cp);
            }
        }
    }
}

void dealerPlay(shoe_t shoe, hand_t hand, float* prob, float inputProb) {
    if (inputProb == 0) {
        char key[11];
        hashShoe(shoe, key);
        int idx = dealerCacheFind(key);
        if (idx < 0) {
            _dealerPlay(shoe, hand, prob, inputProb);
            dealerCacheAdd(key, prob);
        } else {
            memcpy(prob, dealerCache.dprob[idx], sizeof(float) * 10);
        }
    } else {
        _dealerPlay(shoe, hand, prob, inputProb);
    }
}

float playerSplit(shoe_t shoe, hand_t hand, hand_t dealerHand, int nsplits)
{
    //assert();
    hand_t hand1;

    hand1.cards[0] = hand.cards[0]; hand1.length = 1;

    float ev[10] = {0};

    for (int c = 0; c < 10; ++c) {
        if (shoe.cards[c] > 0) {
            shoe_t sh = shoe; // clone
            hand_t h1 = hand1;
            float cp = (float)sh.cards[c]/sh.left;
            handDrawCard(&h1, &sh, c);
            int h1v = handValue(h1);
            h1v = max(h1v/100,h1v%100);
            if (h1v == 21) {
                float *dprob = calloc(10, sizeof(float));
                dealerPlay(sh, h1, dprob, 0);
                for (int i = 0; i < 10; ++i) {
                    int dhv = i+17;
                    if (h1v > dhv || dhv > 21) {
                        ev[c] += 1 * dprob[i];
                    }
                }
                free(dprob);
            } else {
                float standEV = playerStand(sh, h1, dealerHand);
                float hitEV = playerHit(sh, h1, dealerHand);
                float doubleEV = playerDouble(sh, h1, dealerHand);
                ev[c] = maxf(maxf(standEV, hitEV), doubleEV);
                if (c == h1.cards[0] && nsplits < 3) {
                    float splitEV = playerSplit(sh, h1, dealerHand, nsplits + 1);
                    ev[c] = maxf(ev[c], splitEV);
                }
            }
            ev[c] *= cp;
        }
    }
    return sumfa(ev, 10) * 2;
}

float _playerDouble(shoe_t shoe, hand_t playerHand, hand_t dealerHand)
{
    float ev[10] = {0};
    for (int c = 0; c < 10; ++c) {
        if (shoe.cards[c] > 0) {
            shoe_t sh = shoe; // clone
            hand_t ph = playerHand;
            hand_t dh = dealerHand;
            float cp = (float)sh.cards[c]/sh.left;
            handDrawCard(&ph, &sh, c);
            int v = handValue(ph);
            v = max(v/100, v%100);

            if (v > 21) {
                ev[c] = -2;
            } else {
                float *dprob = calloc(10, sizeof(float));
                dealerPlay(sh, dh, dprob, 0);
                for (int i = 0; i < 10; ++i) {
                    int dhv = i+17;
                    if (v > dhv || dhv > 21) {
                        ev[c] += 2 * dprob[i];
                    } else if (v < dhv) {
                        ev[c] += -2 * dprob[i];
                    }
                }
                free(dprob);
            }
            ev[c] *= cp;
        }
    }

    return sumfa(ev, 10);
}


/**
 * @brief playerDouble
 * @param shoe
 * @param playerHand
 * @param dealerHand
 * @return float expected value
 */
float playerDouble(shoe_t shoe, hand_t playerHand, hand_t dealerHand)
{
    char key[15] = {0};
    float ev = 0;
    hashPlayerActionEV(DOUBLE_DOWN, shoe, playerHand, dealerHand, key);
    int idx = playerCacheFind(key);
    if (idx < 0) {
        ev = _playerDouble(shoe, playerHand, dealerHand);
        playerCacheAdd(key, ev);
    } else {
        ev = playerCache.ev[idx];
    }
    return ev;
}

float _playerStand(shoe_t shoe, hand_t playerHand, hand_t dealerHand)
{
    char key[15] = {0};
    float ev = 0;
    hashPlayerActionEV(STAND, shoe, playerHand, dealerHand, key);
    int idx = playerCacheFind(key);
    if (idx < 0) {
        ev = _playerStand(shoe, playerHand, dealerHand);
        playerCacheAdd(key, ev);
    } else {
        ev = playerCache.ev[idx];
    }
    return ev;
}

/**
 * @brief playerStand
 * @param shoe
 * @param playerHand
 * @param dealerHand
 * @return float expected value
 */
float playerStand(shoe_t shoe, hand_t playerHand, hand_t dealerHand)
{
    float ev = 0;
    shoe_t sh = shoe; // clone
    hand_t ph = playerHand;
    hand_t dh = dealerHand;
    int v = handValue(ph);
    v = max(v/100, v%100);

    float *dprob = calloc(10, sizeof(float));

    dealerPlay(sh, dh, dprob, 0);

    for (int i = 0; i < 10; ++i) {
        int dhv = i+17;
        if (v > dhv || dhv > 21) {
            ev += 1 * dprob[i];
        } else if (v < dhv) {
            ev += -1 * dprob[i];
        }
    }

    free(dprob);

    return ev;
}



float _playerHit(shoe_t shoe, hand_t playerHand, hand_t dealerHand)
{
    float cev[10] = {0};
    float hitEV = 0;
    float standEV = 0;

    for (int c = 0; c < 10; ++c) {
        if (shoe.cards[c] > 0) {
            shoe_t sh = shoe; // clone
            hand_t ph = playerHand;
            hand_t dh = dealerHand;
            float cp = (float)sh.cards[c]/sh.left;
            handDrawCard(&ph, &sh, c);
            int v = handValue(ph);
            v = max(v/100, v%100);

            if (v == 21) {
                float *dprob = calloc(10, sizeof(float));
                dealerPlay(sh, dh, dprob, 0);
                cev[c] = 0;
                for (int i = 0; i < 10; ++i) {
                    int dhv = i+17;
                    if (v > dhv || dhv > 21) {
                        cev[c] += 1 * dprob[i];
                    } else if (v < dhv) {
                        cev[c] += -1 * dprob[i];
                    }
                }
                free(dprob);
            } else if (v > 21) {
                cev[c] = -1;
            } else {
                hitEV = playerHit(sh, ph, dh);
                standEV = playerStand(sh, ph, dh);
                cev[c] = maxf(hitEV, standEV);
            }
            cev[c] *= cp;
        }
    }

    return sumfa(cev, 10);
}

float playerHit(shoe_t shoe, hand_t playerHand, hand_t dealerHand)
{
    char key[15] = {0};
    float ev = 0;
    hashPlayerActionEV(HIT, shoe, playerHand, dealerHand, key);
    int idx = playerCacheFind(key);
    if (idx < 0) {
        ev = _playerHit(shoe, playerHand, dealerHand);
        playerCacheAdd(key, ev);
    } else {
        ev = playerCache.ev[idx];
    }
    return ev;
}

void playerPlay(shoe_t shoe, game_t game, float *ev)
{
    bool options[5] = {false};
//    float ev[5] = {0};

    for (int a = 0; a < 5; ++a) {
        if (a == SURRENDER) {
            ev[SURRENDER] = -0.5;
        }
        if (a == STAND) {
            ev[STAND] = playerStand(shoe, game.playerHands[0], game.dealerHand);
        }
        if (a == DOUBLE_DOWN) {
            ev[DOUBLE_DOWN] = playerDouble(shoe, game.playerHands[0], game.dealerHand);
        }
        if (a == HIT) {

        }
        if (a == SPLIT) {

        }
    }
}

float deal(shoe_t shoe, game_t game, int c1, int c2, int c3, int c4) {
    float prob = (float)shoe.cards[c1] /shoe.left;
    shoe.cards[c1]--; shoe.left--;
    prob *= (float)shoe.cards[c2] / shoe.left;
    shoe.cards[c2]--; shoe.left--;

    if (c1 != c2) prob *= 2;

    prob *= (float)shoe.cards[c3] / shoe.left;
    shoe.cards[c3]--; shoe.left--;
    prob *= (float)shoe.cards[c4] / shoe.left;
    shoe.cards[c4]--; shoe.left--;

    return prob;
}

float start(shoe_t shoe, game_t game) {
    float totalEV = 0;
    float weights[10][10][10][10] = { 0 };
    game.playerSplits = 0;
    for (int c1 = 0; c1 < 10; ++c1) {
        game.playerHands[0].cards[0] = c1;
        game.playerHands[0].length = 1;
        for (int c2 = 0; c2 < 10; ++c2) {
            game.dealerHand.cards[0] = c2;
            game.dealerHand.length = 1;
            for (int c3 = c1; c3 < 10; ++c3) {
                game.playerHands[0].cards[1] = c3;
                game.playerHands[0].length = 2;
                for (int c4 = 0; c4 < 10; ++c4) {
                    game.dealerHand.cards[2] = c4;
                    game.dealerHand.length = 2;
                    weights[c1<c3?c1:c3][c1>c3?c1:c3][c2][c4] += deal(shoe, game,
                            game.playerHands[0].cards[0],
                            game.playerHands[0].cards[1],
                            game.dealerHand.cards[0], game.dealerHand.cards[1]);
                }
            }
        }
    }

    for (int c1 = 0; c1 < 10; ++c1) {
        for (int c2 = c1; c2 < 10; ++c2) {
            for (int c3 = 0; c3 < 10; ++c3) {
                for (int c4 = 0; c4 < 10; ++c4) {
                    float ev = 0;
                    if (c1 == 0 && c2 == 9) {
                        if ((c3 == 0 && c4 == 9) || (c3 == 9 && c4 == 0)) {
                            ev = 0; // tie BJ
                        } else {
                            ev = 1.5;
                        }
                    } else if ((c3 == 0 && c4 == 9) || (c3 == 9 && c4 == 0)) {
                        ev = -1;
                    } else {
//                        ev = playerTurn(shoe, game);
                    }
                    printf("%d, %d, %d, %d, %.10f, %0.2f\n", c1+1, c2+1, c3+1, c4+1, weights[c1][c2][c3][c4], ev);
                }
            }
        }
    }

    return totalEV;
}

void initShoe(shoe_t * shoe, int decks) {
//    for (int i = 0; i < 9; ++i) {
//        shoe->cards[i] = 0;
//    }
//    shoe->cards[2] = 1;
//    shoe->cards[7] = 1;
//    shoe->cards[5] = 1;

//    shoe->cards[9] = 2;
//    shoe->cards[1] = 1;
//    shoe->cards[3] = 1;

//    shoe->left = 7;
    for (int i = 0; i < 9; ++i) {
        shoe->cards[i] = 4 * decks;
    }
    shoe->cards[9] = 4*4*decks;
    shoe->left = 52*decks;
}

void initGame(game_t* g)
{
    g->dealerHand.length = 0;
    g->playerSplits = 0;
}


int main()
{
    shoe_t shoe;
    initShoe(&shoe, 8);

    game_t game;
    initGame(&game);

//    start(shoe, game);
//    hand_t h;
//    h.length = 2; h.cards[0] = 0; h.cards[1] = 9; h.cards[2] = 0; h.cards[3] = 8;
//    h.cards[4] = 0;
//    printf("value: %d\n", handValue(h));

    handDrawCard(&(game.dealerHand), &shoe, 4);

    handDrawCard(&(game.playerHands[0]), &shoe, 0);
    handDrawCard(&(game.playerHands[0]), &shoe, 0);
//    handDrawCard(&(game.dealerHand), &shoe, 9);


    float standEV = playerStand(shoe, game.playerHands[0], game.dealerHand);
    printf("STAND: %.8f\n", standEV);

    float hitEV = playerHit(shoe, game.playerHands[0], game.dealerHand);
    printf("HIT: %.8f\n", hitEV);

    float doubleEV = playerDouble(shoe, game.playerHands[0], game.dealerHand);
    printf("DOUBLE: %.8f\n", doubleEV);

    if (game.playerHands[0].cards[0] == game.playerHands[0].cards[1]) {
        float splitEV = playerSplit(shoe, game.playerHands[0], game.dealerHand, 0);
        printf("SPLIT: %.8f\n", splitEV);
    }


//    handDrawCard(&(game.dealerHand), &shoe, 2);
//    handDrawCard(&(game.dealerHand), &shoe, 9);
//    float *prob = calloc(10, sizeof(float));
//    dealerPlay(shoe, game, prob, 0);

//    for (int i = 0; i < 10; ++i) {
//        printf("%d: %.18f\n", i+17, prob[i]);
//    }

//    free(prob);
    return 0;
}
