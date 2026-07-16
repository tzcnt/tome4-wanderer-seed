#ifndef md5_h
#define md5_h

#define HASHSIZE 16

#ifdef __cplusplus
extern "C" {
#endif

void md5(const char *message, long len, char *output);

#ifdef __cplusplus
}
#endif

#endif
