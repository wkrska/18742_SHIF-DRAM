filename="sad_plus"
# filename="bitweave-buddy"

sizes=("8" "16" "32" "64" "128" "256" "512" "1024" "2048" "4096" "8192" "16384" "32768" "65536")
# Loop through each argument
for s in "${sizes[@]}"; do
    cd microworkloads/
    gcc -DSIZE="$s" -static -std=c99 -O3 -msse2 -I ../gem5/util/m5 m5op_x86.S rowop.S** "$filename".c -o "$filename".exe
    cd .. 
    cd gem5/
    ./build/X86/gem5.opt configs/example/se.py --cpu-type=detailed --caches --l2cache --mem-type=DDR4_2400_x64 --mem-size=8192MB -c /home/18872_SHIF-DRAM/microworkloads/"$filename".exe -o > ../temp_results/"$s".txt
    cd ..
done
    
