link-c-sdk示例工程
xx-sample默认基于Linux x86-64
1.示例包xxx-sample.tar.gz获取，解压
2.编译
  进入工程目录sample路径(xxx-sample/sample),执行make，成功编译后当前目录下生成sample文件。如果重新编译，请先执行make distclean
3.示例运行
  3.1 将示例用到的DAK/DSK通过环境变量导入,如下
    export LINK_TEST_DAK="your dak"
    export LINK_TEST_DSK="your dsk"
  3.2 当前路径下(xxx-sample)，运行sample可执行程序，将上传material文件加下的测试文件到服务端。
    ./sample
4.登录portal.qiniu.com查看演示结果
  视频缩略图：xxx-sample/sample/material/3c.jpg
  视频片段的视频：xxx-sample/sample/material/h265_aac_1_16000_h264.h264
  视频片段的音频：xxx-sample/sample/material/h265_aac_1_16000_a.aac
 
