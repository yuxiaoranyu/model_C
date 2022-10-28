谷歌breakpad的qt wrapper，用于在程序崩溃时生成微软minidump文件，可追溯每个线程的堆栈信息。

具体使用方法如下：

1.生成崩溃dmp文件需要在Qt工程pro文件引入依赖文件

include(../common/dump/CrashManager.pri)

2.main.cpp和主函数中添加代码指定dmp文件生成路径为程序所在目录

#include "../common/dump/CrashHandler/CrashHandler.h"
CrashManager::CrashHandler::instance()->Init(QApplication::applicationDirPath());

3.分析dmp文件的工具程序需要去sensor\common\dump\breakpad目录手动编译

./configure CXXFLAGS=-std=c++0x
make

4.切换至程序生成目录，执行以下命令，生成符号表（符号表文件名必须和程序名称一致）

cd /mnt/hgfs/sensor/sensor_pressure
./build.sh board
./../common/dump/breakpad/src/tools/linux/dump_syms/dump_syms ./sensor_pressure > sensor_pressure.sym

5.查看符号表第一行，拷贝十六进制字符串（CE7C0368C125E850C8C983389921BC360）用于创建符号目录

head -n1 sensor_pressure.sym

6.创建符号目录并将符号表放进去

mkdir -p ./symbols/sensor_pressure/CE7C0368C125E850C8C983389921BC360
mv sensor_pressure.sym ./symbols/sensor_pressure/CE7C0368C125E850C8C983389921BC360

7.在客户环境下，若发生dump，将小盒子产生的dmp文件拷贝到ubuntu当前目录(/mnt/hgfs/sensor/sensor_pressure)，结合符号目录，生成崩溃堆栈日志

./../common/dump/breakpad/src/processor/minidump_stackwalk 37d8b788-29d9-3739-6253ede6-7408a671.dmp ./symbols > crash.log

8.打开crash.log，堆栈信息含义说明（release版本产生的日志文件中的函数名可能与源代码不一致，但是能看见行号）

sensor_pressure!crash() [Main.cpp : 64 + 0x3]
编译单元        |        |          |
                函数名   |          |
                         代码文件名 |
                                    行号

9.版本制作时，不用提前准备符号表，只需要将客户环境出问题的可执行文件和对应的dmp文件拷贝出来就能进行错误定位