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
timebase = 0.0
with open('iperf_result_50.txt') as f:
    for line in f:
        l = re.findall('([0-9]+[\.0-9]*)', line)
        xs_50.append(float(l[1]))
        ys_50.append(float(l[4]))

flag = 0
timebase = 0.0
with open('iperf_result_100.txt') as f:
    for line in f:
        l = re.findall('([0-9]+[\.0-9]*)', line)
        xs_100.append(float(l[1]))
        ys_100.append(float(l[4]))

flag = 0
timebase = 0.0
with open('iperf_result_150.txt') as f:
    for line in f:
        l = re.findall('([0-9]+[\.0-9]*)', line)
        xs_150.append(float(l[1]))
        ys_150.append(float(l[4]))


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
plt.ylabel('Bandwidth(Mbps)')
plt.legend(loc='upper right')
plt.show()