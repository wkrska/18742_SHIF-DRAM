# filename="sad_baseline"
filename="sad_plus"

# Test Params
sizes=("8" "16" "32" "64" "128" "256" "512" "1024" "2048" "4096" "8192" "16384" "32768" "65536" "131072" "262144" "524288" "1048576")
dwidth=("8" "16" "32")

# Loop through each argument
for s in "${sizes[@]}"; do
    for d in "${dwidth[@]}"; do
        gcc -DSELECT=0 -DSIZE="$s" -DDWIDTH="$d" -static -std=c99 -O3 -msse2 -I ../gem5/util/m5 m5op_x86.S rowop.S** "$filename".c -o "$filename".exe
        cd ../gem5/
        # ./build/X86/gem5.opt configs/example/se.py --cpu-type=detailed --caches --l2cache --mem-type=DDR4_2400_x64 --mem-size=8192MB -c /home/18872_SHIF-DRAM/microworkloads/"$filename".exe -o "10 1" > ../microworkloads/temp_results/"$s"_"$d".txt
        ./build/X86/gem5.opt configs/example/se.py --cpu-type=detailed --caches --l2cache --mem-type=DDR4_2400_x64 --mem-size=8192MB -c /home/MIMDRAM/18872_SHIF-DRAM/microworkloads/"$filename".exe -o "10 1"> ../microworkloads/temp_results/"$s"_"$d".txt
        cd ../microworkloads/
    done
done

# Aggregate Results using script
python aggregate_results.py > temp_results/aggregate_results.txt