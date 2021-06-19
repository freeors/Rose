bzip2
======
bzip2-1.0.6

boost
======
1_76_0(<boost>/boost/version.hpp)
----
不要用boost::function/boost::bind，改用std::function/std::bind，混用两种bind会给编程较大麻烦。C++11已支持std::function/std::bind。

gettext
=====
gettext include gettext and libiconv directory. compile require Preprocessor Definitions 
-------------------------
XCode(iOS/Mac OS X)
_LIBICONV_H   avoid include system defined <iconv.h>
---------------------------

jpeg-9b
=====
为什么要加入jpeg？webrtc时，从摄像头出来的图像是mjpeg格式，windows端需要有这个解码这格式。Android因为相机模块解码了，不必包括jpeg。

--Preprocessor Definitions
  HAVE_JPEG AVOID_TABLES(Android需要)
要编译的源文件特点：1）一定是以j开始。2）没有jpegtran.c。3）jmem开始的文件中，一定须要jmemmgr.c，windows加jmemnobs.c，android加jmem-android
为不向include增加一个目录，修改<src>\apps\external\third_party\libyuv\source\mjpeg_decoder.cc，
#include <jpeglib.h> ==> #include <jpeg-9b/jpeglib.h>

protobuf
=====
说明：http://www.libsdl.cn/bbs/forum.php?mod=viewthread&tid=149&extra=page%3D1

third_party/ffmpeg
======
Only header. Place here instead with linker/include, because webrtc quire this location.

zlib
======
zlib1.2.8

zxing
=====
zxing来自https://github.com/glassechidna/zxing-cpp), 为什么没有用https://github.com/nu-book/zxing-cpp? --实测下来，在2017年MacBook Pro上。

识别1280x720视频流（没二维码）。nu-book/zxing-cpp：每帧平均花费80毫秒（fast=false时则要300多毫秒）。glassechidna/zxing-cpp：平均70毫秒。
循环识别同一张960x1280图像（二维码：260x260）。nu-book/zxing-cpp：平均55毫秒。glassechidna/zxing-cpp：平均75毫秒。
循环识别同一张260x260图像（整个是二维码，须旋转）。nu-book/zxing-cpp：平均6毫秒。glassechidna/zxing-cpp：平均5毫秒。
循环识别同一张260x260图像（整个是二维码，已正放）。nu-book/zxing-cpp：平均6毫秒。glassechidna/zxing-cpp：平均5毫秒。

当图像中没有二维码时，glassechidna/zxing-cpp要比nu-book快10秒毫。当图像中有二维码，而且nu-book至少不会比glassechidna差。测试下来，在成功识别率上，nu-book极可能要好过glassechidna。为使用nu-book，最好先精准扣出二维码，这会极大加

A：为什么第一次crop时要向内缩10像素。
Q：旋转后会没有部分会有“黑色”填充，在破坏这些“黑色”自连成一个轮廓。

让我很纳闷，在android，直接把摄像头图像输入nu-book/zxing-cpp时，花的时间大概45毫秒，作为3所检测阶段，应该还能忍受。为此，在QrParse::FindQrPoint效果没做到比nu-book/zxing-cpp更优时，把直接输入nu-book/zxing-cpp作为优先算法。

SDL/libvpx
=====
在用的这版本，ios编译很烦索，但好在目前不会用vp8、vp9。一旦要用vp8、vp9，一定要升级webrtc，连接着升libvpx。
