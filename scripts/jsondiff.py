#!/usr/bin/env python3
"""Structural diff of two JSON files: prints each differing path. Usage: jsondiff.py a.json b.json"""
import json, sys
a = json.load(open(sys.argv[1])); b = json.load(open(sys.argv[2]))
out = []
def diff(x, y, path=''):
    if type(x) != type(y): out.append((path, repr(x)[:70], repr(y)[:70])); return
    if isinstance(x, dict):
        for k in sorted(set(x) | set(y)): diff(x.get(k), y.get(k), path + '/' + k)
    elif isinstance(x, list):
        if len(x) != len(y): out.append((path, 'len', len(x), len(y)))
        for i, (p, q) in enumerate(zip(x, y)): diff(p, q, path + f'[{i}]')
    elif x != y: out.append((path, repr(x)[:70], repr(y)[:70]))
diff(a, b)
for o in out[:60]: print(*o)
print(len(out), 'differences')
