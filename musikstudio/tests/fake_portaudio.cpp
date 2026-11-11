// SPDX-License-Identifier: GPL-3.0-or-later

// Fake PortAudio driver for deterministic CI tests. Never ships in .wapp.

#include <algorithm>

#include <atomic>

#include <chrono>

#include <cmath>

#include <cstdlib>

#include <thread>

#include <vector>

struct TestTimeInfo {double inputBufferAdcTime,currentTime,outputBufferDacTime;};

using Callback=int (*)(const void*,void*,unsigned long,const TestTimeInfo*,unsigned long,void*);

struct TestParameters {int device,channelCount;unsigned long sampleFormat;double suggestedLatency;void* hostApiSpecificStreamInfo;};

struct TestStream {

 int in=0,out=0;

 std::atomic<bool> started{false};

 std::thread deviceThread;

 Callback cb=nullptr;

 void* cbUser=nullptr;

 unsigned long frames=512;

 double rate=48000;

};

extern "C" {

struct FakeDeviceInfo {

 int version;const char* name;int hostApi;int maxInput;int maxOutput;

 double lowIn,lowOut,highIn,highOut,rate;

};

struct FakeHostInfo {int version;int type;const char* name;int count,defaultIn,defaultOut;};

static const FakeDeviceInfo microphone{2,"Test Microphone",0,1,0,.02,.02,.1,.1,48000};

static const FakeDeviceInfo speakers{2,"Test Speakers",0,0,2,.02,.02,.1,.1,48000};

static const FakeHostInfo host{1,0,"Wenuy Test Backend",2,0,1};

int Pa_Initialize(){return std::getenv("WENUY_FAKE_AUDIO_FAIL")?-9996:0;}

int Pa_GetDeviceCount(){return 2;}

int Pa_GetDefaultInputDevice(){return std::getenv("WENUY_FAKE_NO_INPUT")?-1:0;}

int Pa_GetDefaultOutputDevice(){return std::getenv("WENUY_FAKE_NO_OUTPUT")?-1:1;}

const FakeDeviceInfo* Pa_GetDeviceInfo(int i){return i==0?&microphone:i==1?&speakers:nullptr;}

const FakeHostInfo* Pa_GetHostApiInfo(int i){return i==0?&host:nullptr;}

int Pa_Terminate(){return 0;}

int Pa_OpenDefaultStream(void** stream,int in,int out,unsigned long fmt,double rate,unsigned long,void*,void*){

 if(std::getenv("WENUY_FAKE_OPEN_FAIL"))return -9996;

 if(rate<8000||rate>192000||fmt!=1||((in==1&&out==0)||(in==0&&out==2))==false)return -9996;

 auto* result=new TestStream;

 result->in=in;result->out=out;result->rate=rate;*stream=result;

 return 0;

}

int Pa_OpenStream(void** stream,const TestParameters* in,const TestParameters* out,double rate,

                  unsigned long frames,unsigned long,Callback cb,void* user){

 if(std::getenv("WENUY_FAKE_OPEN_FAIL"))return -9996;

 if(in||!out||out->device!=1||out->channelCount!=2||out->sampleFormat!=1||

    rate<8000||rate>192000||frames<64||frames>4096||!cb)return -9996;

 auto* result=new TestStream;

 result->in=0;result->out=2;result->rate=rate;result->frames=frames;

 result->cb=cb;result->cbUser=user;*stream=result;

 return 0;

}

int Pa_StartStream(void* stream){

 auto* s=static_cast<TestStream*>(stream);s->started.store(true);

 if(s->cb){

  s->deviceThread=std::thread([s]{

   const auto requested=std::getenv("WENUY_FAKE_FAST_CALLBACK")?8192UL:s->frames;

   std::vector<float> scratch(requested*2);

   const TestTimeInfo t{};

   while(s->started.load()){

    const int state=s->cb(nullptr,scratch.data(),requested,&t,0,s->cbUser);

    if(state!=0){s->started.store(false);break;}

    if(std::getenv("WENUY_FAKE_FAST_CALLBACK"))

     std::this_thread::sleep_for(std::chrono::microseconds(100));

    else{

     const auto delay=std::max(1L,static_cast<long>(std::lround(s->frames*1000.0/s->rate)));

     std::this_thread::sleep_for(std::chrono::milliseconds(delay));

    }

   }

  });

 }

 return 0;

}

int Pa_IsStreamActive(void* stream){return static_cast<TestStream*>(stream)->started.load()?1:0;}

int Pa_StopStream(void* stream){

 auto* s=static_cast<TestStream*>(stream);s->started.store(false);

 if(s->deviceThread.joinable())s->deviceThread.join();

 return 0;

}

int Pa_CloseStream(void* stream){

 Pa_StopStream(stream);

 delete static_cast<TestStream*>(stream);

 return 0;

}

int Pa_WriteStream(void* stream,const void* data,unsigned long frames){

 if(!static_cast<TestStream*>(stream)->started.load()||!data||frames==0)return -9999;

 std::this_thread::sleep_for(std::chrono::milliseconds(1));return 0;

}

int Pa_ReadStream(void* stream,void* data,unsigned long frames){

 if(!static_cast<TestStream*>(stream)->started.load()||!data||frames==0)return -9999;

 auto* samples=static_cast<float*>(data);

 for(unsigned long i=0;i<frames;++i)samples[i]=static_cast<float>(0.1*std::sin(i*0.01));

 std::this_thread::sleep_for(std::chrono::milliseconds(1));return 0;

}

const char* Pa_GetErrorText(int){return "Fake PortAudio device error";}

}
