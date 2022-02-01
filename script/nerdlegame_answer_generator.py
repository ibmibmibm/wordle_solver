#!/usr/bin/env python
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2022 Shen-Ta Hsieh

from itertools import product
from re import compile

o = compile("[-+*/]{2}")
b = compile(r"^[-+*/]")
e = compile(r"[-+*/]$")
z = compile(r"\b0[0-9]")
for f in product("1234567890+-*/", repeat=8):
    h = "".join(f)
    if o.search(h):
        continue
    for i in range(1, 7):
        l = "".join(f[:i])
        if b.search(l):
            continue
        if e.search(l):
            continue
        if z.search(l):
            continue
        r = "".join(f[i:])
        if b.search(r):
            continue
        if e.search(r):
            continue
        if z.search(r):
            continue
        try:
            lv = eval(l)
            rv = eval(r)
        except (SyntaxError, ZeroDivisionError):
            continue
        if lv == rv:
            print("{}={}".format(l, r))
