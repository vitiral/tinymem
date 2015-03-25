'''Conclusion: Use 1 or 4. I am going to use 4

// Designed to create an ultra fast hash with < 256 bins
uint8_t hash(tm_index value, uint8_t mod){
    uint32_t h = value * prime;
    return ((h>>16) xor (h & 0xFFFF)) % mod;
}

def hash4(value):
    h = (value * prime) & mask32
    return xor(((h >> 16) & mask32), h & mask16) & mask32
'''


from operator import xor, itemgetter
import numpy as np
from statistics import mean, stdev

mask8 = 0xff
mask16 = 0xffff
mask32 = 0xffffffff
init = 2166136261
prime = 1677619


def hash(value):
    '''standard fowler-Noll-Vo 16 bit hash
    http://www.isthe.com/chongo/tech/comp/fnv/'''
    h = (init * prime) & mask32
    h = xor(h, value & mask8) & mask32
    h = xor(((h >> 16) & mask32), h & mask16) & mask32
    h = (h * prime) & mask32
    return xor(h, (value >> 8) & mask8) & mask32


def hash2(value):
    h = (init * prime) & mask32
    h = xor(h, value) & mask32
    h = xor(((h >> 16) & mask32), h & mask16) & mask32
    return (h * prime) & mask32


def hash3(value):
    return (value * prime) & mask32


def hash4(value):
    h = (value * prime) & mask32
    return xor(((h >> 16) & mask32), h & mask16) & mask32


def shash(value, size=256):
    return hash(value) % size


def shash2(value, size=256):
    return hash2(value) % size


def shash3(value, size=256):
    return hash3(value) % size


def shash4(value, size=256):
    return hash4(value) % size


def testhash(hfun, values=range(2**16 - 1), size=None):
    fmat = "Testing {:20} size: {:5} Bins: {:5d} Min: {:5d} Max: {:5d}"
    bins = {}
    for n in values:
        if size: h = hfun(n, size)
        else: h = hfun(n)
        if h in bins:
            bins[h].append(n)
        else:
            bins[h] = [n]
    values = (len(bins), min(map(len, bins.values())),
              max(map(len, bins.values())))
    # print(fmat.format(hfun.__name__, str(size), *values))
    l, mn, mx = values
    return (mx - mn),  l

if __name__ == '__main__':
    data = range(0, 2**16 - 1)
    # testhash(hash, data)
    # testhash(hash2, data)
    # testhash(hash3, data)
    # testhash(hash4, data)


    # for s in (33, 55, 77, 100, 256):
    for n in (32, 64, 128):
        hashes = (shash, shash2, shash3, shash4)
        spread = tuple([] for _ in range(4))
        print("Bins: ", n)
        for s in range(1, 3000):
            data = range(0, 2**16 - 1, s)
            # print("Skip:", s)
            for i, h in enumerate(hashes):
                spread[i].append(testhash(h, data, n))

        bins = tuple(map(itemgetter(1), spread))
        spread = tuple(map(itemgetter(0), spread))
        print("Spread mean:", list(map(mean, spread)))
        print("Spread min :", list(map(min, spread)))
        print("Spread max :", list(map(max, spread)))
        print("Spread stdev :", list(map(stdev, spread)))

        print("bins mean:", list(map(mean, bins)))
        print("bins min :", list(map(min, bins)))
        print("bins max :", list(map(max, bins)))
        print("bins stdev :", list(map(stdev, bins)))
