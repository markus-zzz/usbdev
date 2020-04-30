#!/usr/bin/env python
import sys
import re

pattern_j = re.compile("^usb_signalling-1: J$")
pattern_k = re.compile("^usb_signalling-1: K$")
pattern_se0 = re.compile("^usb_signalling-1: SE0$")
pattern_se1 = re.compile("^usb_signalling-1: SE1$")

for line in sys.stdin:
  if (pattern_j.match(line)):
    print('01')
  elif (pattern_k.match(line)):
    print('10')
  elif (pattern_se0.match(line)):
    print('00')
  elif (pattern_se1.match(line)):
    print('11')

