
'''
Script used to log the received data from the FerrarisReader and store and/or display it.
'''


import argparse
import serial
import time


CHOICES = {
    'turns': 'MEAS:TICK',
    'active': 'MEAS:SENS:ACTI',
    'dark': 'MEAS:SENS:DARK',
    'effective': 'MEAS:SENS:EFFE',
    'snr': 'MEAS:SENS:SNR',
    'energy': 'CALC:ENER',
    'power': 'CALC:POW',
    'period': 'MEAS:TICK:PER',
    'last': 'MEAS:TICK:LAST',
    'frequency': 'SYST:FREQ',
}

parser = argparse.ArgumentParser()
parser.add_argument('device')
parser.add_argument('--channels', default=2, type=int,
                    help='number of channels connected to a ferraris meter')
parser.add_argument('--quantities', nargs='+', choices=list(CHOICES.keys()),
                    default=['turns', 'effective', 'energy', 'power'],
                    help='quantities that will be read from each channel')
parser.add_argument('--log', default=None, type=str, help='filename to write to')
parser.add_argument('--quiet', action='store_true',
                    help='If set, no output will be generated')
args = parser.parse_args()


quantities_per_channel = [CHOICES.get(name) for name in args.quantities]
quantities = []
for channel in range(args.channels):
    quantities += ['%s? %d' % (name, channel) for name in quantities_per_channel]

device = serial.Serial(port=args.device)


if args.log:
    logfile = open(args.log, 'w')

try:
    while True:
        data = [str(time.time())]
        for q in quantities:
            device.write((q + '\n').encode('ascii'))
            data.append(device.readline().decode('ascii').strip())
            time.sleep(0.01)
        line = ', '.join(data)
        if not args.quiet:
            print(line)
        if args.log:
            logfile.write(line + '\n')
            logfile.flush()
        time.sleep(1 - 0.01 * len(quantities))

except KeyboardInterrupt:
    pass
finally:
    if args.log:
        logfile.close()
