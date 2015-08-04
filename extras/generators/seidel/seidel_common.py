#!/usr/bin/python

import itertools

def partition_left_right(lst, n):
    for x in itertools.combinations(lst, n):
        yield (list(x), filter(lambda y: y not in x, lst))

def geom_name(degrees):
    names = ["point", "line", "plane"]

    if degrees < 3:
        return names[degrees]

    return "hyperplane"
