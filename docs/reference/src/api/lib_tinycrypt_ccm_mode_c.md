# lib/tinycrypt/ccm_mode.c

## `static void ccm_cbc_mac(uint8_t *T, const uint8_t *data, unsigned int dlen, unsigned int flag, TCAesKeySched_t sched) {`


Variation of CBC-MAC mode used in CCM.


---

## `static int ccm_ctr_mode(uint8_t *out, unsigned int outlen, const uint8_t *in, unsigned int inlen, uint8_t *ctr, const TCAesKeySched_t sched) {`


Variation of CTR mode used in CCM.
The CTR mode used by CCM is slightly different than the conventional CTR
mode (the counter is increased before encryption, instead of after
encryption). Besides, it is assumed that the counter is stored in the last
2 bytes of the nonce.


---

