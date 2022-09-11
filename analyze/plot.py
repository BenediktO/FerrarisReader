import argparse
import matplotlib.pyplot as plt
import matplotlib.dates


import numpy as np


import datetime


def to_datetime(values):
    return np.array([datetime.datetime.fromtimestamp(value) for value in values])

def to_date(values):
    return np.array([datetime.datetime.combine(datetime.datetime.fromtimestamp(value).date(), datetime.time(0, 0)) for value in values])

def to_time(values):
    return np.array([datetime.datetime.combine(datetime.datetime.fromtimestamp(0), datetime.datetime.fromtimestamp(value).time()) for value in values])


CONVERTERS = {
    'datetime': to_datetime,
    'date': to_date,
    'time': to_time,
}

def filter_since(time):
    def inner(values):
        if time == 'today':
            starttime = datetime.datetime.combine(values[-1].date(), datetime.time(0, 0))
        elif time == 'day':
            starttime = datetime.datetime.combine(datetime.datetime.now().date(), datetime.time(0, 0))
        else:
            raise ValueError('Invalid selection %r' % time)
        return values >= starttime
    return inner

def filter_until(time):
    def inner(values):
        starttime = values[-1] - datetime.timedelta(seconds=time)
        print(starttime, values[-1])
        return values >= starttime
    return inner

FILTERS = {
    'day': filter_since('day'),
    'today': filter_since('today'),
    '24h': filter_until(24 * 3600),
    '12h': filter_until(12 * 3600),
    'hour': filter_until(1 * 3600),
}


parser = argparse.ArgumentParser()
parser.add_argument('filename')
parser.add_argument('-x', type=int, nargs='+', default=0)
parser.add_argument('-y', type=int, nargs='+', default=1)
parser.add_argument('--xlabel', type=str, default=None)
parser.add_argument('--ylabel', type=str, default=None)
parser.add_argument('--xconvert', choices=list(CONVERTERS.keys()), default=None)
parser.add_argument('--yconvert', choices=list(CONVERTERS.keys()), default=None)
parser.add_argument('--xfilter', choices=list(FILTERS.keys()), default=None)
#parser.add_argument('--yfilter', choices=[], default=None)
parser.add_argument('--save', type=str, default=None)
args = parser.parse_args()

if len(args.x) == 1 and len(args.y) > 1:
    args.x = [args.x[0]] * len(args.y)
elif len(args.x) > 1 and len(args.y) == 1:
    args.y = [args.y[0]] * len(args.x)
elif len(args.x) == len(args.y):
    pass
else:
    raise ValueError('Both x and y have to be either scalar or vectors of the same length')

if args.xfilter is not None and args.xconvert not in ['datetime', 'date', 'time']:
    raise ValueError('x filtering is only available with a time quantity')

def convert(value):
    try:
        return float(value)
    except:
        return 0


with open(args.filename) as f:
    data = [[convert(value) for value in line.strip().split(',')] for line in f if line]


for x_index, y_index in zip(args.x, args.y):
    values = np.array(data)
    x = values[:,x_index]
    y = values[:,y_index]

    if args.xconvert is not None:
        converter = CONVERTERS[args.xconvert]
        x = converter(x)

    if args.yconvert is not None:
        converter = CONVERTERS[args.yconvert]
        y = converter(y)

    if args.xfilter is not None:
        filter_ = FILTERS[args.xfilter]
        mask = filter_(x)
        x = x[mask]
        y = y[mask]

    if args.xconvert in ['datetime', 'date', 'time'] and args.yconvert is None:
        x = matplotlib.dates.date2num(x)
        plt.plot_date(x, y, fmt='-')
    else:
        plt.plot(x, y)

plt.xlabel(args.xlabel)
plt.ylabel(args.ylabel)

if args.save is not None:
    plt.savefig(args.save)
else:
    plt.show()
