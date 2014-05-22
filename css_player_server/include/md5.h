/* 
 * File:   md5.h
 * Author: root
 *
 * Created on April 17, 2014, 11:30 PM
 */

#ifndef MD5_H
#define	MD5_H

#ifdef	__cplusplus
extern "C" {
#endif
#include <stdint.h>

/*!\file
 * \brief MD5 digest functions
 */

struct MD5Context {
	uint32_t buf[4];
	uint32_t bits[2];
	unsigned char in[64];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
	       unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);
void MD5Transform(uint32_t buf[4], uint32_t const in[16]);


#ifdef	__cplusplus
}
#endif

#endif	/* MD5_H */
