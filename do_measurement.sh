#!/bin/bash
#This script tweak the system setting to minimize the unstable factors while
#analyzing the performance of fibdrv.
#origin code: KYG-yaya573142
#ref:https://github.com/KYG-yaya573142/fibdrv/blob/master/do_measurement.sh

CPUID=15
ORIG_ASLR=`cat /proc/sys/kernel/randomize_va_space`
ORIG_GOV=`cat /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor`
ORIG_TURBO=`cat /sys/devices/system/cpu/intel_pstate/no_turbo`

sudo bash -c "echo 0 > /proc/sys/kernel/randomize_va_space"
sudo bash -c "echo performance > /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor"
sudo bash -c "echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo"

# measure the performance of fibdrv
make
make unload
# generate the fibdrv.ko
make load
# time testing
make client_plot
# use 15th core to execute
sudo taskset -c 15 ./client_plot > plot_input
# gnuplot the result
gnuplot plot.gp
make unload

# restore the original system settings
sudo bash -c "echo $ORIG_ASLR >  /proc/sys/kernel/randomize_va_space"
sudo bash -c "echo $ORIG_GOV > /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor"
sudo bash -c "echo $ORIG_TURBO > /sys/devices/system/cpu/intel_pstate/no_turbo"