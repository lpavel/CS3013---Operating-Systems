typedef struct {
  int iFrom; /* who sent the message (0 .. number-of-threads) */
  int type; /* its type */
  int value1; /* first value */
  int value2; /* second value */
} msg;
