cls
echo "Test: help msg"
python driver.py "cmake-build-debug\scheduler.exe -h" "python scheduler.py -h"

echo
echo "Test: compute"
python driver.py "cmake-build-debug\scheduler.exe -l 10,12,10 -c" "python scheduler.py -l 10,12,10 -c"

echo
echo "Test: non-compute list"
python driver.py "cmake-build-debug\scheduler.exe -l 10,12,10" "python scheduler.py -l 10,12,10"

echo
echo "Test: quantum set"
python driver.py "cmake-build-debug\scheduler.exe -q 4 -l 10,12,10 -p RR -c" "python scheduler.py -q 4 -l 10,12,10 -p RR -c"

echo
echo "Test: different quantum"
python driver.py "cmake-build-debug\scheduler.exe -q 10 -l 10,12,10 -p RR -c" "python scheduler.py -q 10 -l 10,12,10 -p RR -c"

echo
echo "Test: SJF"
python driver.py "cmake-build-debug\scheduler.exe -l 10,12,10 -p SJF -c" "python scheduler.py -l 10,12,10 -p SJF -c"

echo
echo "Test: Maxlen"
cc=$(./cmake-build-debug/scheduler.exe -s 0 -m 9 -c | grep -oE 'length = [1-9]' | wc -l)
if [ "$cc" -eq 3 ]; then
  echo "Test passed"
else
  echo "Test failed!"
fi
