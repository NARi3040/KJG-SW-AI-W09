#ifndef __LIB_ROUND_H
#define __LIB_ROUND_H

/* X를 STEP의 가장 가까운 배수로 올림한 값을 반환함.
 * X >= 0, STEP >= 1 일 때만 유효함. */
#define ROUND_UP(X, STEP) (((X) + (STEP) - 1) / (STEP) * (STEP))

/* X를 STEP으로 나눈 값을 올림해서 반환함.
 * X >= 0, STEP >= 1 일 때만 유효함. */
#define DIV_ROUND_UP(X, STEP) (((X) + (STEP) - 1) / (STEP))

/* X를 STEP의 가장 가까운 배수로 내림한 값을 반환함.
 * X >= 0, STEP >= 1 일 때만 유효함. */
#define ROUND_DOWN(X, STEP) ((X) / (STEP) * (STEP))

/* DIV_ROUND_DOWN은 없음. 그냥 X / STEP 이면 되기 때문임. */

#endif /* lib/round.h */
