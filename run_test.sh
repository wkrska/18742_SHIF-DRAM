filename="my_test"
# filename="bitweave-buddy"

cd microworkloads/
gcc -static -std=c99 -O3 -msse2 -I ../gem5/util/m5 m5op_x86.S rowop.S** "$filename".c -o "$filename".exe
cd .. 
cd gem5/
./build/X86/gem5.opt configs/example/se.py --cpu-type=detailed --caches --l2cache --mem-type=DDR4_2400_x64 --mem-size=8192MB -c /home/MIMDRAM/18872_SHIF-DRAM/microworkloads/"$filename".exe -o "10 1"
cd ..