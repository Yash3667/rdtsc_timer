# rdtsc_timer
A high precision timer using x86 `rdtscp` instruction.

The time stamps taken by `clock_gettime` are not calibrated. The reason is because most people who use `clock_gettime` to get timer statistics do not calibrate the timer, and the results go to show how much of a difference using a calibrated `rdtsc_timer` can make.

Future work: Add support for machines which only support `rdtsc` and not `rdtscp`.