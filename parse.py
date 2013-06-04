#!/usr/bin/python

import sys, string
import os
list = os.listdir("/home/mars/Downloads/tracesim/output/")
list.sort()

f2 = open('outputfile', 'w')
for filename in list:
    f1 = open('./output/'+filename, 'r')
    f2.writelines(filename+':\n')
    for line in f1:
        if "#" in line:
            f2.writelines(line)
    f2.write('\n')
f1.close()
f2.close()
