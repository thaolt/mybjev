#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include "tinycthread.h"


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


mtx_t mtxPlayerCache;
mtx_t mtxDealerCache;


int min(int a, int b) {
    return a>b?b:a;
}

int max(int a, int b) {
    return a>b?a:b;
}

float maxf(float a, float b) {
    return a>b?a:b;
}


int sumia(int *a, int l) {
    int r = 0;
    for (int i = 0; i < l; ++i) {
        r += a[i];
    }
    return r;
}


float sumfa(float *a, int l) {
    float r = 0;
    for (int i = 0; i < l; ++i) {
        r += a[i];
    }
    return r;
}


void dealerCacheAdd(char * key, float *prob)
{
    mtx_lock(&mtxDealerCache);
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

    dealerCache.indexes[dealerCache.len] = calloc(12, sizeof(char));
    memcpy(dealerCache.indexes[dealerCache.len], key, 12 * sizeof(char));
    dealerCache.dprob[dealerCache.len] = calloc(10, sizeof(float));
    memcpy(dealerCache.dprob[dealerCache.len], prob, 10 * sizeof(float));

    dealerCache.len++;
    mtx_unlock(&mtxDealerCache);
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

void hashShoe(int c, shoe_t shoe, char *hash) {
    hash[0] = c;
    for (int i = 0; i < 10; ++i) {
        hash[i+1] = shoe.cards[i]+1;
    }
    hash[11] = 0;
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


void playerCacheAdd(char * key, float ev)
{
    mtx_lock(&mtxPlayerCache);
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
    mtx_unlock(&mtxPlayerCache);
}

int playerCacheFind(char* key) {
    for (size_t i = 0; i < playerCache.len; ++i) {
        if (strcmp(key, playerCache.indexes[i]) == 0) {
            return i;
        }
    }
    return -1;
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
        if (shoe.cards[c] > 0
            && ((hand.cards[0] != 0 || hand.length != 1 || c != 9)
            && (hand.cards[0] != 9 || hand.length != 1 || c != 0))
        ) {
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
        char key[12];
        hashShoe(hand.cards[0], shoe, key);
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

typedef struct {
    int c;
    shoe_t sh;
    hand_t ph;
    hand_t dh;
    int nsplits;
    float *ret;
} playerSplitThreadArgument_t;


int _playerSplitThread(void* varg)
{
    playerSplitThreadArgument_t *arg = (playerSplitThreadArgument_t *) varg;

    float cp = (float)(arg->sh.cards[arg->c])/arg->sh.left;
    arg->ph.length = 1;
    handDrawCard(&(arg->ph), &(arg->sh), arg->c);
    int h1v = handValue(arg->ph);
    h1v = max(h1v/100,h1v%100);
    if (h1v == 21) {
        float *dprob = calloc(10, sizeof(float));
        dealerPlay(arg->sh, arg->ph, dprob, 0);
        for (int i = 0; i < 10; ++i) {
            int dhv = i+17;
            if (h1v > dhv || dhv > 21) {
                arg->ret[arg->c] += 1 * dprob[i];
            }
        }
        free(dprob);
    } else {
        float standEV = playerStand(arg->sh, arg->ph, arg->dh);
        float hitEV = playerHit(arg->sh, arg->ph, arg->dh);
        float doubleEV = playerDouble(arg->sh, arg->ph, arg->dh);
        arg->ret[arg->c] = maxf(maxf(standEV, hitEV), doubleEV);
        if (arg->c == arg->ph.cards[0] && arg->nsplits < 3) {
            float splitEV = playerSplit(arg->sh, arg->ph, arg->dh, arg->nsplits + 1);
            arg->ret[arg->c] = maxf(arg->ret[arg->c], splitEV);
        }
    }
    arg->ret[arg->c] *= cp;
    return 0;
}

float _playerSplit(shoe_t shoe, hand_t hand, hand_t dealerHand, int nsplits)
{
    hand_t hand1;

    hand1.cards[0] = hand.cards[0]; hand1.length = 1;

    float ev[10] = {0};
    thrd_t threads[10];
    float **ret = calloc(10, sizeof(float*));

    for (int i = 0; i < 10; i++) {
        ret[i] = calloc(10, sizeof(float));
    }

    for (int c = 0; c < 10; ++c) {
        if (shoe.cards[c] > 0) {
            playerSplitThreadArgument_t arg;
            arg.c = c;
            arg.sh = shoe; // clone
            arg.ph = hand;
            arg.dh = dealerHand;
            arg.nsplits = nsplits;
            arg.ret = ret[c];
            thrd_create(&threads[c], _playerSplitThread, (void*) &arg);
        }
    }

    thrd_join(threads[0], NULL);
    thrd_join(threads[1], NULL);
    thrd_join(threads[2], NULL);
    thrd_join(threads[3], NULL);
    thrd_join(threads[4], NULL);
    thrd_join(threads[5], NULL);
    thrd_join(threads[6], NULL);
    thrd_join(threads[7], NULL);
    thrd_join(threads[8], NULL);
    thrd_join(threads[9], NULL);

    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; ++j) {
            ev[i] += ret[i][j];
        }
        free(ret[i]);
    }
    free(ret);

    return sumfa(ev, 10) * 2;
}

float playerSplit(shoe_t shoe, hand_t playerHand, hand_t dealerHand, int nsplits)
{
    float ev = 0;
    char key[15] = {0};
    hashPlayerActionEV(SPLIT, shoe, playerHand, dealerHand, key);
    int idx = playerCacheFind(key);
    if (idx < 0) {
        ev = _playerSplit(shoe, playerHand, dealerHand, nsplits);
        playerCacheAdd(key, ev);
    } else {
        ev = playerCache.ev[idx];
    }
    return ev;
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


/**
 * @brief playerStand
 * @param shoe
 * @param playerHand
 * @param dealerHand
 * @return float expected value
 */
float playerStand(shoe_t shoe, hand_t playerHand, hand_t dealerHand)
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


// mybjev A5 6 12 1 2 3 4 5 6 7 8 9 10

int main(int argc, char **argv)
{
    if (argc < 13) {
        printf("missing arguments\n");
        return 1;
    }

    hand_t ph; ph.length=strlen(argv[1]);
    for (int i = 0; i < ph.length; ++i) {
        ph.cards[i] = argv[1][i] - 48;
    }

    hand_t dh; dh.length=strlen(argv[2]);
    for (int i = 0; i < dh.length; ++i) {
        dh.cards[i] = argv[2][i] - 48;
    }

    shoe_t shoe;
    shoe.left = 0;
    for (int i = 0; i < 10; ++i) {
        shoe.cards[i] = atoi(argv[i+3]);
        shoe.left += shoe.cards[i];
    }

    if (ph.length == 2) {
        printf("SURRENDER: %.8f\n", -0.5);
    }

    float standEV = playerStand(shoe, ph, dh);
    printf("STAND: %.8f\n", standEV);

    float hitEV = playerHit(shoe, ph, dh);
    printf("HIT: %.8f\n", hitEV);

    if (ph.length == 2) {
        float doubleEV = playerDouble(shoe, ph, dh);
        printf("DOUBLE: %.8f\n", doubleEV);
    }

    if (ph.length == 2 && ph.cards[0] == ph.cards[1]) {
        float splitEV = playerSplit(shoe, ph, dh, 0);
        printf("SPLIT: %.8f\n", splitEV);
    }

    return 0;
}
