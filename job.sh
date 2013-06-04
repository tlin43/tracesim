#!/bin/bash

TRACE_FILE=(
"bwaves-bwaves-bwaves-bwaves-bwaves-bwaves-bwaves-bwaves"
"bwaves-bwaves-bwaves-bwaves-lbm-lbm-lbm-lbm"
"bwaves-bwaves-bwaves-bwaves-libquantum-libquantum-libquantum-libquantum"
"bwaves-bwaves-bwaves-bwaves-mcf-mcf-mcf-mcf"
"bwaves-bwaves-bwaves-bwaves-milc-milc-milc-milc"
"bwaves-bwaves-bwaves-bwaves-soplex-soplex-soplex-soplex"
"bwaves-bwaves-mcf-mcf-libquantum-libquantum-lbm-lbm"
"bwaves-bwaves-mcf-mcf-milc-milc-lbm-lbm"
"bwaves-bwaves-mcf-mcf-milc-milc-libquantum-libquantum"
"bwaves-bwaves-mcf-mcf-milc-milc-soplex-soplex"
"bwaves-bwaves-mcf-mcf-soplex-soplex-lbm-lbm"
"bwaves-bwaves-mcf-mcf-soplex-soplex-libquantum-libquantum"
"bwaves-bwaves-milc-milc-libquantum-libquantum-lbm-lbm"
"bwaves-bwaves-milc-milc-soplex-soplex-lbm-lbm"
"bwaves-bwaves-milc-milc-soplex-soplex-libquantum-libquantum"
"bwaves-bwaves-soplex-soplex-libquantum-libquantum-lbm-lbm"
"lbm-lbm-lbm-lbm-lbm-lbm-lbm-lbm"
"libquantum-libquantum-libquantum-libquantum-lbm-lbm-lbm-lbm"
"libquantum-libquantum-libquantum-libquantum-libquantum-libquantum-libquantum-libquantum"
"mcf-mcf-mcf-mcf-lbm-lbm-lbm-lbm"
"mcf-mcf-mcf-mcf-libquantum-libquantum-libquantum-libquantum"
"mcf-mcf-mcf-mcf-mcf-mcf-mcf-mcf"
"mcf-mcf-mcf-mcf-milc-milc-milc-milc"
"mcf-mcf-mcf-mcf-soplex-soplex-soplex-soplex"
"mcf-mcf-milc-milc-libquantum-libquantum-lbm-lbm"
"mcf-mcf-milc-milc-soplex-soplex-lbm-lbm"
"mcf-mcf-milc-milc-soplex-soplex-libquantum-libquantum"
"mcf-mcf-soplex-soplex-libquantum-libquantum-lbm-lbm"
"milc-milc-milc-milc-lbm-lbm-lbm-lbm"
"milc-milc-milc-milc-libquantum-libquantum-libquantum-libquantum"
"milc-milc-milc-milc-milc-milc-milc-milc"
"milc-milc-milc-milc-soplex-soplex-soplex-soplex"
"milc-milc-soplex-soplex-libquantum-libquantum-lbm-lbm"
"soplex-soplex-soplex-soplex-lbm-lbm-lbm-lbm"
"soplex-soplex-soplex-soplex-libquantum-libquantum-libquantum-libquantum"
"soplex-soplex-soplex-soplex-soplex-soplex-soplex-soplex")


for i in {0..35}
do
	echo "#PBS -N ${TRACE_FILE[$i]}" >> ${TRACE_FILE[$i]}.pbs
	echo "#PBS -o /nv/hp20/tlin43/tracesim/pbs_out/${TRACE_FILE[$i]}.output" >> ${TRACE_FILE[$i]}.pbs
	echo "#PBS -q eceforce-6" >> ${TRACE_FILE[$i]}.pbs
	echo "#PBS -l pmem=2gb" >> ${TRACE_FILE[$i]}.pbs
	echo "#PBS -j oe" >> ${TRACE_FILE[$i]}.pbs
	echo "#PBS -l nodes=1:ppn=1" >> ${TRACE_FILE[$i]}.pbs
	echo "#PBS -l walltime=1:00:00" >> ${TRACE_FILE[$i]}.pbs

	echo "./tracesim.o --cache-size 256 --cache-assoc 32 --core-num 8 --scheme 0 --trace-file /nv/hp20/tlin43/data/trace-with-eip-and-ifetch/${TRACE_FILE[$i]}.dramcache_trace --output-file /nv/hp20/tlin43/tracesim/output/${TRACE_FILE[$i]}_32way.out" >> ${TRACE_FILE[$i]}.pbs

	

#	./tracesim.o --cache-size 256 --cache-assoc 1  --core-num 8 --scheme 0 --trace-file ../trace-with-eip-and-ifetch/${TRACE_FILE[$i]}.dramcache_trace --output-file ./output/${TRACE_FILE[$i]}_01way.out
#	./tracesim.o --cache-size 256 --cache-assoc 32 --core-num 8 --scheme 1 --trace-file ../trace-with-eip-and-ifetch/${TRACE_FILE[$i]}.dramcache_trace --output-file ./output/${TRACE_FILE[$i]}_32way_bypass0.out
#	./tracesim.o --cache-size 256 --cache-assoc 1  --core-num 8 --scheme 1 --trace-file ../trace-with-eip-and-ifetch/${TRACE_FILE[$i]}.dramcache_trace --output-file ./output/${TRACE_FILE[$i]}_01way_bypass0.out

done

for i in {0..35}
do
	qsub ${TRACE_FILE[$i]}.pbs
done

