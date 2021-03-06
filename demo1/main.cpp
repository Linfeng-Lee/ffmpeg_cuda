#include <iostream>

// Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
#ifdef __cplusplus
}
#endif

using namespace std;
int main(int argc, char *argv[])
{
    for (int i = 0; i < argc; i++)
    {
        cout << "argument[" << i << "]:" << argv[i] << endl;
    }

    const AVOutputFormat *ofmt = NULL;
    // Input AVFormatContext and Output AVFormatContext
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;

    const char *in_filename, *out_filename;
    int ret, i;
    int videoindex = -1;
    int frame_index = 0;
    // in_filename  = "rtmp://58.200.131.2:1935/livetv/hunantv";
    if (argv[1] == NULL)
        in_filename = "http://img.ksbbs.com/asset/Mon_1704/15868902d399b87.flv";
    else
        in_filename = argv[1];

    cout<<"in_filename:"<<in_filename<<endl;
    out_filename = "receive.flv";
    // av_register_all();
    // Network
    avformat_network_init();
    /*avformat_open_input params:
        AVFormatContext **ps,
        const char *filename,
        AVInputFormat *fmt,
        AVDictionary **options
    */
    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0)
    {
        printf("Could not open input file.");
        goto end;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0)
    {
        printf("Failed to retrieve input stream information");
        goto end;
    }
    //？ ctx=context
    // nb_streams：输入视频的AVStream个数
    //找到第一个为视频流的位置
    std::cout<<ifmt_ctx->nb_streams<<std::endl;
    exit(0);
    for (i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoindex = i;
            break;
        }
    }
    // av_dump_format,  打印关于输入或输出格式的详细信息, is_output 选择指定的上下文是输入(0)还是输出(1)
    printf("this is stream infomation of informat context ...\n");
    av_dump_format(ifmt_ctx, 0, in_filename, 0);

    // Output
    //根据out_filename自动推导输出格式
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    if (!ofmt_ctx)
    {
        printf("Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    ofmt = ofmt_ctx->oformat;
    for (i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        // Create output AVStream according to input AVStream
        AVStream *in_stream = ifmt_ctx->streams[i];
        // AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
        const AVCodec *codec = avcodec_find_decoder(in_stream->codecpar->codec_id);
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, codec);

        if (!out_stream)
        {
            printf("Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        // 申请AVCodecContext空间。需要传递一个编码器，也可以不传，但不会包含编码器。
        AVCodecContext *p_codec_ctx = avcodec_alloc_context3(codec);
        //avcodec_parameters_to_context: 该函数用于将流里面的参数，也就是AVStream里面的参数直接复制到AVCodecContext的上下文当中。
        ret = avcodec_parameters_to_context(p_codec_ctx, in_stream->codecpar);

        // Copy the settings of AVCodecContext
        if (ret < 0)
        {
            printf("Failed to copy context from input to output stream codec context\n");
            goto end;
        }

        p_codec_ctx->codec_tag = 0;
        
        // 初始化文件头信息
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            p_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        ret = avcodec_parameters_from_context(out_stream->codecpar, p_codec_ctx);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "eno:[%d] error to paramters codec paramter \n", ret);
        }
    }

    printf("this is stream infomation of outformat context ...\n");
    // Dump Format------------------
    av_dump_format(ofmt_ctx, 0, out_filename, 1);
    // Open output URL
    if (!(ofmt->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            printf("Could not open output URL '%s'", out_filename);
            goto end;
        }
    }
    // Write file header
    //params:
    // s：用于输出的AVFormatContext。
    // options：额外的选项，目前没有深入研究过，一般为NULL。
    // 函数正常执行后返回值等于0。
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0)
    {
        printf("Error occurred when opening output URL\n");
        goto end;
    }

    //拉流
    while (1)
    {
        AVStream *in_stream, *out_stream;
        // Get an AVPacket
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0)
            break;

        in_stream = ifmt_ctx->streams[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];
        /* copy packet */
        // Convert PTS/DTS
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        // Print to Screen
        if (pkt.stream_index == videoindex)
        {

            printf("Receive %8d video frames from input URL\n", frame_index);
            frame_index++;
        }

        // 写packet
        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
        if (ret < 0)
        {
            printf("Error muxing packet\n");
            break;
        }
        av_packet_unref(&pkt);
    }

    // Write file trailer
    av_write_trailer(ofmt_ctx);
end:
    avformat_close_input(&ifmt_ctx);
    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF)
    {
        printf("Error occurred.\n");
        return -1;
    }
    return 0;
}