import re
import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np

xs_t = []
ys_t = []
xs_r = []
ys_r = []
xs_c = []
ys_c = []

flag = 0
timebase = 0.0
with open('codel/rtt.txt') as f:
    for line in f:
        l = re.split(', |\n', line)
        if flag == 0:
            timebase = float(l[0])
            flag = 1
        xs_c.append(float(l[0]) - timebase)
        rtt_list = re.findall('time=([0-9]+[\.0-9]*)', line)
        ys_c.append(float(rtt_list[0]))

flag = 0
timebase = 0.0
with open('red/rtt.txt') as f:
    for line in f:
        l = re.split(', |\n', line)
        if flag == 0:
            timebase = float(l[0])
            flag = 1
        xs_r.append(float(l[0]) - timebase)
        rtt_list = re.findall('time=([0-9]+[\.0-9]*)', line)
        ys_r.append(float(rtt_list[0]))

flag = 0
timebase = 0.0
with open('taildrop/rtt.txt') as f:
    for line in f:
        l = re.split(', |\n', line)
        if flag == 0:
            timebase = float(l[0])
            flag = 1
        xs_t.append(float(l[0]) - timebase)
        rtt_list = re.findall('time=([0-9]+[\.0-9]*)', line)
        ys_t.append(float(rtt_list[0]))

x_c = np.array(xs_c)
y_c = np.array(ys_c)
x_r = np.array(xs_r)
y_r = np.array(ys_r)
x_t = np.array(xs_t)
y_t = np.array(ys_t)

plt.yscale('log')
l_c = plt.plot(x_c, y_c, label='Codel')
l_r = plt.plot(x_r, y_r, label='RED')
l_t = plt.plot(x_t, y_t, label='Tail Drop')
plt.xlabel('Time(s)')
plt.ylabel('rtt(s)')
plt.legend(loc='upper right')
plt.show()