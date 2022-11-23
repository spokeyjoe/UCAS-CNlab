import re
import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np

xs_50 = []
ys_50 = []
xs_100 = []
ys_100 = []
xs_150 = []
ys_150 = []

flag = 0
first = 0.0
with open('qlen-50/qlen.txt') as f:
    for line in f:
        l = re.split(', |\n', line)
        if flag == 0:
            first = float(l[0])
            flag = 1
        xs_50.append(float(l[0]) - first)
        ys_50.append(float(l[1]))

flag = 0
with open('qlen-100/qlen.txt') as f:
    for line in f:
        l = re.split(', |\n', line)
        if flag == 0:
            first = float(l[0])
            flag = 1
        xs_100.append(float(l[0]) - first)
        ys_100.append(float(l[1]))

flag = 0
with open('qlen-150/qlen.txt') as f:
    for line in f:
        l = re.split(', |\n', line)
        if flag == 0:
            first = float(l[0])
            flag = 1
        xs_150.append(float(l[0]) - first)
        ys_150.append(float(l[1]))

x_50 = np.array(xs_50)
y_50 = np.array(ys_50)
x_100 = np.array(xs_100)
y_100 = np.array(ys_100)
x_150 = np.array(xs_150)
y_150 = np.array(ys_150)

l_50 = plt.plot(x_50, y_50, label='maxq=50')
l_100 = plt.plot(x_100, y_100, label='maxq=100')
l_150 = plt.plot(x_150, y_150, label='maxq=150')
plt.xlabel('Time(s)')
plt.ylabel('Qlen')
plt.legend(loc='upper right')
plt.show()