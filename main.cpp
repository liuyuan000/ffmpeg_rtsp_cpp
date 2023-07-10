#include <iostream>
#include<chrono>
#include<thread>
#include<mutex>
#include <opencv2/opencv.hpp>
extern "C"   //ffmpeg是采用c语言实现的 c++工程种 导入c语言
{
    //avcodec:编解码(最重要的库)
#include <libavcodec/avcodec.h>
//avformat:封装格式处理
#include <libavformat/avformat.h>
//swscale:视频像素数据格式转换
#include <libswscale/swscale.h>
//avdevice:各种设备的输入输出
#include <libavdevice/avdevice.h>
//avutil:工具库（大部分库都需要这个库的支持）
#include <libavutil/avutil.h>

#include <libavutil/imgutils.h>

}

std::mutex frame_lock;

std::queue<cv::Mat> frame_queue;

void thread_read()
{
    AVDictionary* options = 0;
    avformat_network_init();
    std::cout << "start work" << std::endl;

    av_dict_set(&options, "buffer_size", "655360", 0); //设置缓存大小 655360 平衡 4096000 延迟大 不卡
    av_dict_set(&options, "rtsp_transport", "tcp", 0);  //以tcp的方式打开,
    av_dict_set(&options, "stimeout", "5000000", 0);    //设置超时断开链接时间，单位us,   5s
    av_dict_set(&options, "max_delay", "500000", 0);    //设置最大时延
    //av_dict_set(&options, "thread_queue_size", "30", 0);    //设置接收线程数据队列大小

    // Open RTSP stream
    AVFormatContext* formatContext = nullptr;

    if (avformat_open_input(&formatContext, "rtsp://192.168.0.100:6554/live/test0", nullptr, &options) != 0) {
        //if (avformat_open_input(&formatContext, "rtsp://192.168.0.100:6554/live/test0", nullptr, nullptr) != 0) {
        std::cerr << "Failed to open RTSP stream." << std::endl;
    }
    formatContext->probesize = 42000000;

    std::cout << "probesize " << formatContext->probesize << std::endl;

    // Find stream info
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        std::cerr << "Failed to find stream info." << std::endl;
    }

    // Find video stream index
    int videoStreamIndex = -1;
    for (unsigned int i = 0; i < formatContext->nb_streams; ++i) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }

    if (videoStreamIndex == -1) {
        std::cerr << "Failed to find video stream." << std::endl;   
    }

    // Get codec parameters for the video stream
    AVCodecParameters* codecParameters = formatContext->streams[videoStreamIndex]->codecpar;

    // Find video decoder
    const AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);
    if (codec == nullptr) {
        std::cerr << "Failed to find video decoder." << std::endl; 
    }

    // Allocate codec context
    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(codecContext, codecParameters) < 0) {
        std::cerr << "Failed to allocate codec context." << std::endl;  
    }
    codecContext->thread_count = 4;
    codecContext->thread_type = FF_THREAD_SLICE;

    // Open codec
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Failed to open codec." << std::endl;
    }


    AVPacket packet;
    cv::Mat frame;

    frame = cv::Mat(codecContext->height, codecContext->width, CV_8UC3);

    AVFrame* pFrameRGB = av_frame_alloc();
    uint8_t* buffer = (uint8_t*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecContext->width, codecContext->height, 1));
    av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, codecContext->width, codecContext->height, 1);

    struct SwsContext* sws_ctx = sws_getContext(codecContext->width, codecContext->height, codecContext->pix_fmt,
        codecContext->width, codecContext->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    AVFrame* avFrame = av_frame_alloc();
    while (true) {
        auto start = std::chrono::high_resolution_clock::now();
        int ret = av_read_frame(formatContext, &packet);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start); // 计算耗时
        std::cout << "av_read_frame " << duration.count() / 1000 << " ms" << std::endl;
        if (ret >= 0 && packet.stream_index == videoStreamIndex) {
            // Decode video frame
            ret = avcodec_send_packet(codecContext, &packet);
            if (ret == AVERROR(EAGAIN)) // 缓冲区已满，要从内部缓冲区读取解码后的音视频帧
            {
                printf("<DecodePktToFrame> avcodec_send_frame() EAGAIN\n");
            }
            else if (ret == AVERROR_EOF) // 数据包送入结束不再送入,但是可以继续可以从内部缓冲区读取解码后的音视频帧
            {
                printf("<DecodePktToFrame> avcodec_send_frame() AVERROR_EOF\n");
            }
            else if (ret < 0)  // 送入输入数据包失败
            {
                printf("<DecodePktToFrame> [ERROR] fail to avcodec_send_frame(), res=%d\n", ret);
               
            }
            else if (ret == 0)
            {
               
                ret = avcodec_receive_frame(codecContext, avFrame);

                if (ret == 0) {
                    // Convert frame to RGB
                    int h = sws_scale(sws_ctx, avFrame->data, avFrame->linesize, 0, codecContext->height, pFrameRGB->data, pFrameRGB->linesize);
                    if (h == codecContext->height)
                    {
                        frame.data = pFrameRGB->data[0];
                        if (frame_queue.size() < 5)
                        {
                            cv::Mat frame_cp;
                            frame.copyTo(frame_cp);
                            frame_lock.lock();
                            frame_queue.push(frame_cp);
                            //frame_queue.push(frame);
                            frame_lock.unlock();
                        }
                        std::cout << "1frame_queue size " << frame_queue.size() << std::endl;
                    } 
                }
            }
        }
        av_packet_unref(&packet);
        
    }
    av_frame_free(&avFrame);

    // Clean up
    avformat_close_input(&formatContext);
    avformat_network_deinit();
    avcodec_free_context(&codecContext);
    sws_freeContext(sws_ctx);
}

int main() {
    // Initialize FFmpeg
    //av_register_all();//在新版本4.0以后，不需要调用该方法，可以直接使用所有模块。
    cv::Mat frame;
    std::thread t1(thread_read);
    // Create OpenCV window for displaying frames
    cv::namedWindow("RTSP Stream", cv::WINDOW_NORMAL);
    t1.detach();
    while (true)
    {
        if (frame_queue.size() > 0)
        {
            // Display frame
            frame_lock.lock();
            frame = frame_queue.front();
            frame_queue.pop();
            frame_lock.unlock();

            cv::imshow("RTSP Stream", frame);
            cv::waitKey(5);
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));  //毫秒
        }
    }
    

    return 0;
}
