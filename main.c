#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

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
    int cards[20];
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

void dealerPlay(shoe_t shoe, hand_t hand, float* prob, float inputProb);



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

float favg(float *a, int l)
{
    float r = (float) a[0];
    for (int i = 1; i < l; ++i) {
        r += a[i];
        r /= 2;
    }
    return r;
}


float compute(shoe_t shoe, game_t game) {
    return 0;
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

void dealerPlay(shoe_t shoe, hand_t hand, float* prob, float inputProb)
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
            handDrawCard(&h, &sh, c);
            int ndhv = handValue(h);
            int hv = ndhv / 100;
            int sv = ndhv % 100;
            float cp = (float)(sh.cards[c]+1)/(sh.left+1); // card probability
            if (sv >= 17 || hv >= 17) {
                if (inputProb == 0)
                    prob[ max(hv,sv)-17 ] += cp;
                else
                    prob[ max(hv,sv)-17 ] += inputProb * cp;
            } else {
                if (inputProb == 0)
                    dealerPlay(sh, h, prob, cp);
                else
                    dealerPlay(sh, h, prob, inputProb * cp);
            }
        }
    }
}

float playerSplits(shoe_t shoe, game_t game) {

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
    float ev[10] = {0};
    for (int c = 0; c < 10; ++c) {
        if (shoe.cards[c] > 0) {
            shoe_t sh = shoe; // clone
            hand_t ph = playerHand;
            hand_t dh = dealerHand;
            float cp = (float)shoe.cards[c]/shoe.left;
            handDrawCard(&ph, &shoe, c);
            int v = handValue(ph);
            v = max(v/100, v%100);

            float *dprob = calloc(10, sizeof(float));

            dealerPlay(sh, dh, dprob, 0);

            for (int i = 0; i < 10; ++i) {
                int dhv = i+17;
                if (v > dhv || dhv > 21) {
                    ev[c] += 2 * dprob[i];
                } else if (v == dhv) {
                    ev[c] += 0 * dprob[i];
                } else { // if (v < dhv)
                    ev[c] += -2 * dprob[i];
                }
            }

            free(dprob);

            ev[c] *= cp;
        }
    }

    return sumfa(ev, 10);
}

float playerPlay(shoe_t shoe, game_t game) {
    bool options[5] = {false};
    float ev = 0;

    shoe.left -= 4;
    shoe.cards[game.playerHands[0].cards[0]]--;
    shoe.cards[game.playerHands[0].cards[1]]--;
    shoe.cards[game.dealerHand.cards[0]]--;
    shoe.cards[game.dealerHand.cards[1]]--;

    for (int a = 0; a < 5; ++a) {
        if (a == SURRENDER) {
            ev = -0.5;
        }
        if (a == STAND) {
            float *prob = calloc(10, sizeof(float));
//            dealerPlay(shoe, game, prob, 0);
            free(prob);
        }
        if (a == DOUBLE_DOWN) {

        }
        if (a == HIT) {

        }
    }

    return 0;
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


void c2prob(int *cts, float *prob, int l)
{
    int sum = sumia(cts, l);
    for (int i = 0; i < l; ++i) {
        prob[i] = (float)cts[i] / sum;
    }
}

int main()
{
    shoe_t shoe;
    initShoe(&shoe, 12);

    game_t game;
    initGame(&game);

//    start(shoe, game);
//    hand_t h;
//    h.length = 2; h.cards[0] = 0; h.cards[1] = 9; h.cards[2] = 0; h.cards[3] = 8;
//    h.cards[4] = 0;
//    printf("value: %d\n", handValue(h));

    handDrawCard(&(game.playerHands[0]), &shoe, 4);
    handDrawCard(&(game.playerHands[0]), &shoe, 5);
    handDrawCard(&(game.dealerHand), &shoe, 5);
//    handDrawCard(&(game.dealerHand), &shoe, 9);

    float doubleEV = playerDouble(shoe, game.playerHands[0], game.dealerHand);
    printf("%.10f\n", doubleEV);

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
