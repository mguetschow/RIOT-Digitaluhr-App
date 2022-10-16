#ifndef _WATCHY_GATT_H
#define _WATCHY_GATT_H

int watchy_gatt_init (void);

// *buf pointer to an allocated buffer of length len
// returns number of bytes written to buf
// buf does not need to be larger than 21 bytes,
// which is the NUS maxuimum per NUS transfer (plus trailing NUL byte)
int watchy_gatt_nus_get_rx(char *buf, unsigned int len);

ssize_t gatt_svr_nus_tx_buf(char *buf, unsigned int len);

#endif
