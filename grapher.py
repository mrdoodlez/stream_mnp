import sys
#import pyplot etc ....

# the idea behind here is to fetch data from
# stdin in real time and plot what is to be plotted

for line in sys.stdin:
    if line.find("####") == 0:
        # parse and route to pyplot
        pass
    else:
        print(line)