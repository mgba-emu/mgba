// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

#include <mgba-util/hash.h>

#if defined(_MSC_VER)

#define FORCE_INLINE	__forceinline

#include <stdlib.h>

#define ROTL32(x,y)	_rotl(x,y)

#else

#define	FORCE_INLINE inline __attribute__((always_inline))

static inline uint32_t rotl32(uint32_t x, int8_t r) {
	return (x << r) | (x >> (32 - r));
}

#define	ROTL32(x, y) rotl32(x, y)

#endif

//-----------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here

static FORCE_INLINE uint32_t getblock32(const uint32_t* p, ssize_t i) {
	uint32_t ret;
	LOAD_32LE(ret, i << 2, p);
	return ret;
}

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

static FORCE_INLINE uint32_t fmix32(uint32_t h) {
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

//-----------------------------------------------------------------------------

uint32_t hash32(const void* key, size_t len, uint32_t seed) {
	const uint8_t* data = (const uint8_t*) key;
	const int nblocks = len / 4;

	uint32_t h1 = seed;

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	//----------
	// body

	const uint32_t* blocks = (const uint32_t*)(data + nblocks * 4);

	int i;
	for (i = -nblocks; i; i++) {
		uint32_t k1 = getblock32(blocks, i);

		k1 *= c1;
		k1 = ROTL32(k1, 15);
		k1 *= c2;
		
		h1 ^= k1;
		h1 = ROTL32(h1, 13); 
		h1 = h1 * 5 + 0xe6546b64;
	}

	//----------
	// tail

	const uint8_t* tail = (const uint8_t*)(data + nblocks * 4);

	uint32_t k1 = 0;

	switch(len & 3) {
	case 3:
		k1 ^= tail[2] << 16;
		// Fall through
	case 2:
		k1 ^= tail[1] << 8;
		// Fall through
	case 1:
		k1 ^= tail[0];
		k1 *= c1;
		k1 = ROTL32(k1, 15);
		k1 *= c2;
		h1 ^= k1;
	};

	//----------
	// finalization

	h1 ^= len;

	h1 = fmix32(h1);

	return h1;
} 
