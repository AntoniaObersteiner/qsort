#!/usr/bin/python3

import matplotlib.pyplot as plt
import re
import sys

filename = sys.argv[1]
line_regex = re.compile(r".*single_sort_step \[(.*) \.\. (.*)\].*\n")
values = []
with open(filename, "r") as file:
    for line in file:
        if "single_sort_step" in line:
            m = line_regex.fullmatch(line)
            if m:
                values.append(int(m[1], 16))
                values.append(int(m[2], 16))

plt.plot(values, "+")
plotfile = filename + ".svg"
print(f"{plotfile = }")
plt.savefig(plotfile)
