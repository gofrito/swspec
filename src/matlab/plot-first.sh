#!/bin/sh

FFTLEN=131072

fname=results.txt
if [ "$1" != "" ]; then
        fname=$1
fi


FFT2PTS=$(($FFTLEN/2))
FFTHEAD=$(($FFT2PTS+2))

head -$FFTHEAD $fname | tail -$FFT2PTS > data.csv

echo "Generated data.csv"
echo "Plotting to screen and 'plot.ps', press 'Q' to exit gnuplot..."
gnuplot gnuplot_data_csv

cp plot.ps ${fname}.ps
ps2image ${fname}.ps ${fname}.png
