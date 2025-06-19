#include "modules/audio_processing/gain_control_impl.h"
#include "modules/audio_processing/gain_controller2.h"
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "modules/audio_processing/audio_buffer.h"
#include "common_audio/wav_file.h"
#include "common_audio/wav_header.h"
#include "common_audio/channel_buffer.h"
#include "common_audio/include/audio_util.h"
#include "common_audio/test_utils.h"

using namespace std;
using namespace webrtc;

struct Agc2Context
{
    std::unique_ptr<webrtc::GainController2> gain_controller;
    std::unique_ptr<webrtc::AudioBuffer> audio_buffer;
    webrtc::StreamConfig stream_config;
    int samples_per_chunk;
    bool split_bands;
    int num_channels;

    std::unique_ptr<ChannelBuffer<float>> in_buf;
    std::unique_ptr<ChannelBuffer<float>> out_buf;

    void run(std::vector<float> &chunk)
    {
        FloatS16ToFloat(&chunk[0], chunk.size(), &chunk[0]);
        Deinterleave(&chunk[0], in_buf->num_frames(), in_buf->num_channels(),
                     in_buf->channels());

        process(in_buf, out_buf);

        Interleave(out_buf->channels(), out_buf->num_frames(), out_buf->num_channels(),
                   &chunk[0]);
        FloatToFloatS16(&chunk[0], chunk.size(), &chunk[0]);
    }

    void process(const std::unique_ptr<ChannelBuffer<float>> &in = nullptr,
                 const std::unique_ptr<ChannelBuffer<float>> &out = nullptr)
    {

        audio_buffer->CopyFrom(in->channels(), stream_config);

        // 주파수 밴드 분할 (48kHz > 16kHz)
        if (split_bands)
        {
            audio_buffer->SplitIntoFrequencyBands();
        }

        // AGC2 처리
        gain_controller->Process(audio_buffer.get());

        // 주파수 밴드 병합
        if (split_bands)
        {
            audio_buffer->MergeFrequencyBands();
        }

        // 처리된 데이터 추출
        audio_buffer->CopyTo(stream_config, out->channels());
    }
};

class AGC2Context
{
    std::unique_ptr<webrtc::GainController2> gain_controller;
    webrtc::StreamConfig stream_config;
    int samples_per_chunk;
    bool split_bands;
    int num_channels;
    int sample_rate;

    webrtc::AudioProcessing::Config::GainController2 def_config;
    

    std::unique_ptr<webrtc::AudioBuffer> audio_buffer;
    std::unique_ptr<ChannelBuffer<float>> in_buf;
    std::unique_ptr<ChannelBuffer<float>> out_buf;

    WavWriter *out_file = nullptr;
    WavWriter *in_file = nullptr;

public:
    int GetChunkSize() { return samples_per_chunk; }

    AGC2Context(int s_rate, int n_ch
        , float fixed_digital_gain = 3.0f
        , bool en_adaptive_digital = true
        , float vad_pa = 1.0f
        , bool subband = false)
        : sample_rate(s_rate), num_channels(n_ch) 
         , split_bands(subband)
         
    {
        webrtc::AudioProcessing::Config::GainController2 config;
        config.enabled = true;
        config.fixed_digital.gain_db = fixed_digital_gain;
        config.adaptive_digital.enabled = en_adaptive_digital;
        config.adaptive_digital.vad_probability_attack = vad_pa;

        // config.adaptive_digital.level_estimator =
        //     webrtc::AudioProcessing::Config::GainController2::LevelEstimator::kPeak;

        gain_controller = std::make_unique<webrtc::GainController2>();
        gain_controller->Initialize(sample_rate);
        gain_controller->ApplyConfig(config);
        RTC_CHECK_EQ(gain_controller->Validate(config), true);

        const int chunks_per_second = 100; // 10ms chunks

        samples_per_chunk = sample_rate / chunks_per_second;
        stream_config = webrtc::StreamConfig(sample_rate, num_channels);
        // not work for 48KHz (not implemented version?)
        //split_bands = (sample_rate > 16000);

        audio_buffer = std::make_unique<webrtc::AudioBuffer>(
            samples_per_chunk, num_channels,
            samples_per_chunk, num_channels,
            samples_per_chunk);

        in_buf = std::make_unique<webrtc::ChannelBuffer<float>>(
            samples_per_chunk, num_channels);

        out_buf = std::make_unique<webrtc::ChannelBuffer<float>>(
            samples_per_chunk, num_channels);
    }

    void Apply(float gain_db
        , bool en_adaptive_digital 
        , float vad_probability_attack
        )
    {
        webrtc::AudioProcessing::Config::GainController2 config;
        config.enabled = true;
        config.fixed_digital.gain_db = gain_db;
        config.adaptive_digital.enabled = en_adaptive_digital;
        config.adaptive_digital.vad_probability_attack = vad_probability_attack;

        gain_controller->ApplyConfig(config);
    }

    // input: float16 ?
    void Run(std::vector<float> &chunk)
    {
        if (in_file){
            //out_file->WriteMySamples(chunk.data(), chunk.size());
            in_file->WriteSamples(chunk.data(), chunk.size());
        }

        FloatS16ToFloat(&chunk[0], chunk.size(), &chunk[0]);
        Deinterleave(&chunk[0], in_buf->num_frames(), in_buf->num_channels(),
                     in_buf->channels());

        process(in_buf, out_buf);

        Interleave(out_buf->channels(), out_buf->num_frames(), out_buf->num_channels(),
                   &chunk[0]);
        FloatToFloatS16(&chunk[0], chunk.size(), &chunk[0]);
    }

    void RunFloat(std::vector<float> &chunk)
    {
        if (in_file){
            in_file->WriteMySamples(chunk.data(), chunk.size());
            //out_file->WriteSamples(chunk.data(), chunk.size());
        }

        //FloatS16ToFloat(&chunk[0], chunk.size(), &chunk[0]);
        Deinterleave(&chunk[0], in_buf->num_frames(), in_buf->num_channels(),
                     in_buf->channels());

        process(in_buf, out_buf);

        Interleave(out_buf->channels(), out_buf->num_frames(), out_buf->num_channels(),
                   &chunk[0]);
        //FloatToFloatS16(&chunk[0], chunk.size(), &chunk[0]);

        // if (out_file){
        //     out_file->WriteMySamples(chunk.data(), chunk.size());
        //     //out_file->WriteSamples(chunk.data(), chunk.size());
        // }
    }

    void process(const std::unique_ptr<ChannelBuffer<float>> &in = nullptr,
                 const std::unique_ptr<ChannelBuffer<float>> &out = nullptr)
    {

        audio_buffer->CopyFrom(in->channels(), stream_config);

        // 주파수 밴드 분할 (48kHz > 16kHz)
        if (split_bands)
        {
            audio_buffer->SplitIntoFrequencyBands();
        }

        // AGC2 처리
        gain_controller->Process(audio_buffer.get());

        // 주파수 밴드 병합
        if (split_bands)
        {
            audio_buffer->MergeFrequencyBands();
        }

        // 처리된 데이터 추출
        audio_buffer->CopyTo(stream_config, out->channels());
    }

    void NotifyAnalogLevel(int level)
    {
        gain_controller->NotifyAnalogLevel(level);
    }

    void Debug(int id)
    {
        // save
        if (id == 1){
            toggle_wavout(in_file, "/root/agc.float.in.wav");
            toggle_wavout(out_file, "/root/agc.float.out.wav");
        }
    }

    void WriteOutSamples(float *p, int n_samples)
    {
        if (out_file){
            out_file->WriteMySamples(p, n_samples);
        }
    }

    // void WriteBytes(float *p, int bytes)
    // {
    //     if (out_file){
    //         out_file->WriteBytes(p, bytes);
    //     }
    // }




private:  // private method
    void toggle_wavout(WavWriter* &h, string fname = "/root/agc2.out.wav")
    {
        if (h)
        {
            delete h;
            h = nullptr;
        }
        else
        {
            // TODO: support sample fomrat: kInt16
            h = new WavWriter(fname, sample_rate, num_channels, 
                WavFile::SampleFormat::kFloat);
        }
    }
    
};

Agc2Context *agc2_init(int sample_rate, int num_channels,
                       float fixed_gain_db, bool adaptive_enable);
void agc2_process(Agc2Context *ctx, float *pcm_buffer, int num_samples);
void agc2_destroy(Agc2Context *ctx);

Agc2Context *agc2_init(int sample_rate, int num_channels,
                       float fixed_gain_db, bool adaptive_enable)
{

    auto ctx = new Agc2Context;

    // AGC2 설정
    webrtc::AudioProcessing::Config::GainController2 config;
    config.enabled = true;
    config.fixed_digital.gain_db = fixed_gain_db;
    config.adaptive_digital.enabled = adaptive_enable;

    // AGC2 인스턴스 생성
    ctx->gain_controller = std::make_unique<webrtc::GainController2>();
    // ctx->gain_controller.reset(new GainController2);
    ctx->gain_controller->Initialize(sample_rate);
    ctx->gain_controller->ApplyConfig(config);

    RTC_CHECK_EQ(ctx->gain_controller->Validate(config), true);

    // 오디오 버퍼 초기화
    const int chunks_per_second = 100; // 10ms chunks
    ctx->samples_per_chunk = sample_rate / chunks_per_second;
    ctx->stream_config = webrtc::StreamConfig(sample_rate, num_channels);
    ctx->split_bands = false; //(sample_rate > 16000);
    ctx->num_channels = num_channels;

    ctx->audio_buffer = std::make_unique<webrtc::AudioBuffer>(
        ctx->samples_per_chunk, num_channels,
        ctx->samples_per_chunk, num_channels,
        ctx->samples_per_chunk);

#if 1
    ctx->in_buf = std::make_unique<webrtc::ChannelBuffer<float>>(
        ctx->samples_per_chunk, num_channels);

    ctx->out_buf = std::make_unique<webrtc::ChannelBuffer<float>>(
        ctx->samples_per_chunk, num_channels);
#endif

    return ctx;
}

void agc2_destroy(Agc2Context *ctx)
{
    delete ctx;
}

// interleaved pcm_buffer
void agc2_process2(Agc2Context *ctx,
                   const std::unique_ptr<ChannelBuffer<float>> &in_buf,
                   const std::unique_ptr<ChannelBuffer<float>> &out_buf,
                   const std::unique_ptr<WavWriter> &out_file = nullptr)
{
    const int frames_per_chunk = ctx->samples_per_chunk;
    const int num_channels = ctx->num_channels; // 채널 수 가져오기

    ctx->audio_buffer->CopyFrom(in_buf->channels(), ctx->stream_config);

    // 주파수 밴드 분할 (48kHz > 16kHz)
    if (ctx->split_bands)
    {
        ctx->audio_buffer->SplitIntoFrequencyBands();
    }

    // AGC2 처리
    ctx->gain_controller->Process(ctx->audio_buffer.get());

    // 주파수 밴드 병합
    if (ctx->split_bands)
    {
        ctx->audio_buffer->MergeFrequencyBands();
    }

    // 처리된 데이터 추출
    ctx->audio_buffer->CopyTo(ctx->stream_config, out_buf->channels());
}

// interleaved pcm_buffer
void agc2_process_interleaved(Agc2Context *ctx, float *pcm_buffer,
                              int num_samples,
                              const std::unique_ptr<WavWriter> &out_file = nullptr)
{
    const int frames_per_chunk = ctx->samples_per_chunk;
    const int num_channels = ctx->num_channels; // 채널 수 가져오기

    // 인터리브 → 디인터리브 변환 버퍼
    std::vector<std::vector<float>> deinterleaved(num_channels);
    for (auto &ch : deinterleaved)
    {
        ch.resize(frames_per_chunk);
    }

    for (int i = 0; i < num_samples; i += frames_per_chunk * num_channels)
    {
        // 현재 청크의 인터리브 데이터 분리
        for (int f = 0; f < frames_per_chunk; ++f)
        {
            for (int ch = 0; ch < num_channels; ++ch)
            {
                deinterleaved[ch][f] = pcm_buffer[i + f * num_channels + ch];
            }
        }

        // 오디오 버퍼에 복사
        std::vector<float *> ptrs(num_channels);
        for (int ch = 0; ch < num_channels; ++ch)
        {
            ptrs[ch] = deinterleaved[ch].data();
        }
        ctx->audio_buffer->CopyFrom(ptrs.data(), ctx->stream_config);

        // 주파수 밴드 분할 (48kHz > 16kHz)
        if (ctx->split_bands)
        {
            ctx->audio_buffer->SplitIntoFrequencyBands();
        }

        // AGC2 처리
        ctx->gain_controller->Process(ctx->audio_buffer.get());

        // 주파수 밴드 병합
        if (ctx->split_bands)
        {
            ctx->audio_buffer->MergeFrequencyBands();
        }

        // 처리된 데이터 추출
        ctx->audio_buffer->CopyTo(ctx->stream_config, ptrs.data());

        // 디인터리브 → 인터리브 변환
        for (int f = 0; f < frames_per_chunk; ++f)
        {
            for (int ch = 0; ch < num_channels; ++ch)
            {
                pcm_buffer[i + f * num_channels + ch] = deinterleaved[ch][f];
            }
        }
    }
}

const int kChunkSizeMs = 10;
const int kSampleRate16kHz = 16000;
extern "C" int test_main(int argc, char *argv[]);

struct Agcinput
{
    char *input_file;
    char *output_file;
};

/// @brief not work: need ChannelBuffer ??
/// @param agc_input
void my_agc2(struct Agcinput *agc_input)
{
    std::unique_ptr<WavReader> in_file(new WavReader(agc_input->input_file));
    int input_sample_rate_hz = in_file->sample_rate();
    int input_num_channels = in_file->num_channels();

    std::unique_ptr<WavWriter> out_file(new WavWriter(agc_input->output_file,
                                                      input_sample_rate_hz,
                                                      input_num_channels));

    Agc2Context *ctx = agc2_init(input_sample_rate_hz, input_num_channels, 5.0f, true);

    // interleaved pcm
    std::vector<float> chunk;
    chunk.resize(ctx->samples_per_chunk * input_num_channels);

    bool samples_left_process = true;
    int count = 0;
    while (samples_left_process)
    {
        samples_left_process =
            in_file->ReadSamples(chunk.size(), &chunk[0]) == chunk.size();

        // out_file->WriteSamples(chunk.data(), chunk.size());

        agc2_process_interleaved(ctx, chunk.data(), chunk.size(), out_file);

        out_file->WriteSamples(chunk.data(), chunk.size());
        count++;
        // if(count > 10000) break;
        printf(".");
    }
}

// API 함수 선언
extern "C"
{
    void *AGC2_init(int sample_rate, int num_channels,
                    float fixed_gain_db, bool adaptive_enable, float vad_pa)
    {
        AGC2Context *ctx = new AGC2Context(sample_rate, num_channels, 
            fixed_gain_db, adaptive_enable, vad_pa);
        // ctx->Apply(fixed_gain_db, adaptive_enable);
        return ctx;
    }



    /// @brief
    /// @param h
    /// @param pcm_buffer : interleaved pcm buffer
    /// @param bytes : 
    void AGC2_process(void *h, float *pcm_buffer, int bytes)
    {
        AGC2Context *ctx = (AGC2Context *)h;
        std::vector<float> buf = std::vector<float>(pcm_buffer, pcm_buffer + bytes / sizeof(float));
#if 0 // to verify input
    // ok
    //ctx->WriteBytes(pcm_buffer, bytes);

    // ok
    ctx->WriteMySamples(buf.data(), buf.size());

#endif

        //ctx->Run(buf);
        ctx->RunFloat(buf);

        //ctx->WriteOutSamples(buf.data(), buf.size());

        if(pcm_buffer!= buf.data())
            memcpy(pcm_buffer, buf.data(), bytes);
    }




    void AGC2_destroy(void *h)
    {
        AGC2Context *ctx = (AGC2Context *)h;
        delete ctx;
    }




    int AGC2_GetChunkSize(void *h)
    {
        AGC2Context *ctx = (AGC2Context *)h;
        return ctx->GetChunkSize();
    }



    void AGC2_Apply(void *h, float gain_db, bool en_adaptive_digital
        , float vad_probability_attack
        )
    {
        ((AGC2Context *)h)->Apply(gain_db, 
            en_adaptive_digital, vad_probability_attack);
    }



    void AGC2_NotifyAnalogLevel(void *h, int level)
    {
        ((AGC2Context *)h)->NotifyAnalogLevel(level);
    }

    void AGC2_Debug(void *h, int id)
    {
        ((AGC2Context *)h)->Debug(id);
    }
}

void my3_agc2(struct Agcinput *agc_input)
{
    std::unique_ptr<WavReader> in_file(new WavReader(agc_input->input_file));
    int input_sample_rate_hz = in_file->sample_rate();
    int input_num_channels = in_file->num_channels();
    WavFile::SampleFormat wfmt =
        in_file->sample_format() == WavFormat::kWavFormatIeeeFloat ? WavFile::SampleFormat::kFloat : WavFile::SampleFormat::kInt16;

    std::unique_ptr<WavWriter> out_file(new WavWriter(agc_input->output_file,
                                                      input_sample_rate_hz,
                                                      input_num_channels,
                                                      wfmt));

    // #define USE_EXT_CHBUF
    float gain_db = 0;
    AGC2Context *ctx = new AGC2Context(input_sample_rate_hz, input_num_channels);

    // interleaved pcm
    std::vector<float> chunk;
    chunk.resize(ctx->GetChunkSize() * input_num_channels);
    ctx->Debug(1);
    bool samples_left_process = true;
    int count = 0;
    while (samples_left_process)
    {
        samples_left_process =
            in_file->ReadSamples(chunk.size(), &chunk[0]) == chunk.size();

        ctx->Run(chunk);

        out_file->WriteSamples(chunk.data(), chunk.size());

        count++;
        // if(count % 1000 == 0){
        //   ctx->Apply(gain_db+=0.1, true);
        //   printf("gain:%f\n", gain_db);
        // }

        if (count > 10000)
            break;
        printf(".");
    }
    ctx->Debug(1);
}

/// @brief temp ok
/// @param agc_input
void my2_agc2(struct Agcinput *agc_input)
{
    std::unique_ptr<WavReader> in_file(new WavReader(agc_input->input_file));
    int input_sample_rate_hz = in_file->sample_rate();
    int input_num_channels = in_file->num_channels();

    std::unique_ptr<WavWriter> out_file(new WavWriter(agc_input->output_file,
                                                      input_sample_rate_hz,
                                                      input_num_channels));

    // #define USE_EXT_CHBUF

#if defined(USE_EXT_CHBUF)
    std::unique_ptr<ChannelBuffer<float>> in_buf;
    int kChunksPerSecond = 1000 / 10;
    in_buf.reset(new ChannelBuffer<float>(input_sample_rate_hz / kChunksPerSecond, input_num_channels));
    std::unique_ptr<ChannelBuffer<float>> out_buf;
    out_buf.reset(new ChannelBuffer<float>(input_sample_rate_hz / kChunksPerSecond, input_num_channels));
#endif

    Agc2Context *ctx = agc2_init(input_sample_rate_hz, input_num_channels, 5.0f, true);

    // interleaved pcm
    std::vector<float> chunk;
    chunk.resize(ctx->samples_per_chunk * input_num_channels);

    bool samples_left_process = true;
    int count = 0;
    while (samples_left_process)
    {
        samples_left_process =
            in_file->ReadSamples(chunk.size(), &chunk[0]) == chunk.size();

#if defined(USE_EXT_CHBUF)
        FloatS16ToFloat(&chunk[0], chunk.size(), &chunk[0]);
        Deinterleave(&chunk[0], in_buf->num_frames(), in_buf->num_channels(),
                     in_buf->channels());

        ctx->process(in_buf, out_buf);

        Interleave(out_buf->channels(), out_buf->num_frames(), out_buf->num_channels(),
                   &chunk[0]);
        FloatToFloatS16(&chunk[0], chunk.size(), &chunk[0]);
#else
        ctx->run(chunk);
#endif

        out_file->WriteSamples(chunk.data(), chunk.size());

        count++;
        if (count > 10000)
            break;
        printf(".");
    }
}

/// @brief original example :
/// not work for 32K/48KHz:
//  if disable Split/MergeFrequencyBadns, gain work(but side effect??)
/// @param agc_input
void agc2(struct Agcinput *agc_input)
{
    std::unique_ptr<WavReader> in_file(new WavReader(agc_input->input_file));
    int input_sample_rate_hz = in_file->sample_rate();
    int input_num_channels = in_file->num_channels();

    std::unique_ptr<WavWriter> out_file(new WavWriter(agc_input->output_file, input_sample_rate_hz, input_num_channels));
    std::unique_ptr<ChannelBufferWavReader> buffer_reader_;
    buffer_reader_.reset(new ChannelBufferWavReader(std::move(in_file)));

    std::unique_ptr<ChannelBuffer<float>> in_buf_;
    int kChunksPerSecond = 1000 / 10;
    in_buf_.reset(new ChannelBuffer<float>(input_sample_rate_hz / kChunksPerSecond, input_num_channels));

    std::unique_ptr<ChannelBufferWavWriter> buffer_writer_;
    buffer_writer_.reset(new ChannelBufferWavWriter(std::move(out_file)));
    std::unique_ptr<ChannelBuffer<float>> out_buf_;
    out_buf_.reset(new ChannelBuffer<float>(input_sample_rate_hz / kChunksPerSecond, input_num_channels));

    AudioProcessing::Config::GainController2 agc2_config;
    agc2_config.enabled = true;
    agc2_config.adaptive_digital.enabled = true;
    agc2_config.fixed_digital.gain_db = 10;

    std::unique_ptr<GainController2> gainController2;
    gainController2.reset(new GainController2);
    gainController2->Initialize(input_sample_rate_hz);
    gainController2->ApplyConfig(agc2_config);

    RTC_CHECK_EQ(gainController2->Validate(agc2_config), true);
    StreamConfig sc(input_sample_rate_hz, input_num_channels);
    AudioBuffer ab(
        input_sample_rate_hz / kChunksPerSecond, input_num_channels,
        input_sample_rate_hz / kChunksPerSecond, input_num_channels,
        input_sample_rate_hz / kChunksPerSecond);

    bool samples_left_process = true;
    int count = 0;
    while (samples_left_process)
    {
        samples_left_process = buffer_reader_->Read(in_buf_.get());
        ab.CopyFrom(in_buf_->channels(), sc);
        if (input_sample_rate_hz > kSampleRate16kHz)
        {
            ab.SplitIntoFrequencyBands();
        }
        // gainController2->NotifyAnalogLevel(3);
        gainController2->Process(&ab);

        if (input_sample_rate_hz > kSampleRate16kHz)
        {
            ab.MergeFrequencyBands();
        }
        ab.CopyTo(sc, out_buf_->channels());
        buffer_writer_->Write(*out_buf_);
        count++;
        if (count > 30000)
            break;
    }
}

int test_main(int argc, char *argv[])
{
    std::cout << "webrtc audio processing agc2 test" << std::endl;
    char *input_file = argv[1];
    char *output_file = argv[2];

    Agcinput agc_handle;
    agc_handle.input_file = input_file;
    agc_handle.output_file = output_file;
    // agc2(&agc_handle);
    // my_agc2(&agc_handle);
    // my2_agc2(&agc_handle);
    my3_agc2(&agc_handle);
    return 0;
}
