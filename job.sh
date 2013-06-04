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
	./tracesim.o --cache-size 256 --cache-assoc 32 --core-num 8 --scheme 0 --trace-file ./trace/${TRACE_FILE[$i]}.dramcache_trace --output-file ./output/${TRACE_FILE[$i]}_32way.out
	./tracesim.o --cache-size 256 --cache-assoc 1  --core-num 8 --scheme 0 --trace-file ./trace/${TRACE_FILE[$i]}.dramcache_trace --output-file ./output/${TRACE_FILE[$i]}_01way.out
	./tracesim.o --cache-size 256 --cache-assoc 32 --core-num 8 --scheme 1 --trace-file ./trace/${TRACE_FILE[$i]}.dramcache_trace --output-file ./output/${TRACE_FILE[$i]}_32way_bypass0.out
	./tracesim.o --cache-size 256 --cache-assoc 1  --core-num 8 --scheme 1 --trace-file ./trace/${TRACE_FILE[$i]}.dramcache_trace --output-file ./output/${TRACE_FILE[$i]}_01way_bypass0.out

done

