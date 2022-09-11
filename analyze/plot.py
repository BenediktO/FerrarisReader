import argparse
import matplotlib.pyplot as plt


parser = argparse.ArgumentParser()
parser.add_argument('filename')
parser.add_argument('-x', type=int, default=0)
parser.add_argument('-y', type=int, default=1)
parser.add_argument('--save', type=str, default=None)
args = parser.parse_args()


def convert(value):
    try:
        return float(value)
    except:
        return 0


with open(args.filename) as f:
    data = [[convert(value) for value in line.strip().split(',')] for line in f if line]


x = [values[args.x] for values in data]
y = [values[args.y] for values in data]


plt.plot(x, y)
if args.save is not None:
    plt.savefig(args.save)
else:
    plt.show()
