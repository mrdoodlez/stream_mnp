# stream_mnp
Input stream data match and plot

# build
```
> winbuild.cmd
```

# run
```
> randgen.py
> matcher.exe
```
or (grapher is not working in fact)
```
> randgen.py
> matcher.exe | grapher.py
```

# about
* randgen.py - input binary streams generator. creates s0.dat and s1.dat
* matcher.exe - reads s0.dat & s1.dat, feeds data to Matcher thread and produces ("plottable") stdout
* grapher.py - plotter utility (not ready)

Matcher app is dual htread: Fetcher -> Matcher \
Fetcher selects random port and pushes new data chunk \
Matcher asynchronously reads new data from port alignes it and writes to output \
Matcher thread is interrupt driven. No new data - no new output

For additional info please consider source code comments =)

# todo
* error handling
* resource clean-up
* Input stream data rate (not just timestamp!) difference must be taken into considerartion

