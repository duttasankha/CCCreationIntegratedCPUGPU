SHELL := /bin/bash

B=1

all:
	gcc -static -D BUSY_FACTOR=${B} -g -O2 -Wall main.c -o attack -lpthread -lrt
android:
	gcc -static -D BUSY_FACTOR=${B} -D USE_ANDROID -g -O2 -Wall main.c -o attack -lpthread -lrt
flush:
	gcc -D BUSY_FACTOR=1 -D USE_FLUSH=1 -g -O2 -Wall main.c -o attack -lpthread -lrt
nocavity:
	gcc -D BUSY_FACTOR=${B} -D NO_CAVITY=1 -g -O2 -Wall main.c -o attack -lpthread -lrt
	
run:
	sudo ./attack

align:
	svf/align.awk -v encfile=log_encrypt -v encfactor=${B} -v prefix="_" out_nr
#	svf/align.awk -v encfile=log_encrypt -v encfactor=1 -v prefix="_" out_flush_reload

dosvf:
	svf/svf < _svf_trace | tee _svf_result

plot:
	gnuplot -e "prefix='_';" svf/map.gnu

clean:
	sudo rm attack _* out* log*

printmetric:
	csv=$$(gawk '{printf("%f\n",$$2)}' _csv_result);\
	crit=$$(gawk '/^matches/{x=$$2} /^remaining/{y=$$2} END{printf("%f\n",x/(x+y))}' _align_results);\
	fp=$$(gawk '/^matches/{x=$$2} /^mismatches/{y=$$2} END{printf("%f\n",y/(x+y))}' _align_results);\
	printf "csv:$${csv} crit:$${crit} fp:$${fp}\n"

runall:
	make -s CORE_NO=1 hifreq
	make -s CORE_NO=2 hifreq
	make -s CORE_NO=3 hifreq
	make -s run
	make -s CORE_NO=1 ondemand
	make -s CORE_NO=2 ondemand
	make -s CORE_NO=3 ondemand
	make -s align
#	make plot
	make -s printmetric

run100:
	for i in $(shell seq -w 00 99); do \
	  echo $$i; \
	  make flush; \
	  make runall; \
	  cp out_nr $${i}_attacker; \
	  cp log_encrypt $${i}_victim; \
	done

CORE_NO=3


smt-off:
	sudo sh -c "echo 0 > /sys/devices/system/cpu/cpu4/online"
	sudo sh -c "echo 0 > /sys/devices/system/cpu/cpu5/online"
	sudo sh -c "echo 0 > /sys/devices/system/cpu/cpu6/online"
	sudo sh -c "echo 0 > /sys/devices/system/cpu/cpu7/online"

smt-on:
	sudo sh -c "echo 1 > /sys/devices/system/cpu/cpu4/online"
	sudo sh -c "echo 1 > /sys/devices/system/cpu/cpu5/online"
	sudo sh -c "echo 1 > /sys/devices/system/cpu/cpu6/online"
	sudo sh -c "echo 1 > /sys/devices/system/cpu/cpu7/online"

hifreq:
	sudo sh -c "echo userspace > /sys/devices/system/cpu/cpu${CORE_NO}/cpufreq/scaling_governor"
	sudo sh -c "cat /sys/devices/system/cpu/cpu${CORE_NO}/cpufreq/cpuinfo_max_freq > /sys/devices/system/cpu/cpu${CORE_NO}/cpufreq/scaling_setspeed"

ondemand:
	sudo sh -c "echo ondemand > /sys/devices/system/cpu/cpu${CORE_NO}/cpufreq/scaling_governor"

