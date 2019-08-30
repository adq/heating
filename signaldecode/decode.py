#!/bin/python

import csv
import sys


for line in open(sys.argv[1]):
    line = line.strip()
    if not line:
        continue


    print("-----------------------------")
    acc = 0
    bitcount = 0
    for bit in line.split(','):
        bit = bit.strip()
        if not bit:
            break

        bit = int(bit)
        acc <<= 1
        acc |= bit
        bitcount += 1

        if bitcount >= 8:
            print(hex(acc))
            bitcount = 0
            acc = 0
