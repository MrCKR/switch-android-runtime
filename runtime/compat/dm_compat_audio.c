/*
 * Android AudioToolbox shim backed by AAudio.
 *
 * This file is intentionally a sink adapter, not a second audio queue. The
 * portable upstream HLE queue remains in hle_audio.c: AAudio calls its render
 * callback, which pulls PCM blocks, moves released buffers, and signals the
 * guest event exactly as the Apple AudioUnit callback did.
 *
 * AAudio uses a callback with (stream, userData, audioData, frameCount), while
 * AudioUnit uses an AudioBufferList. The only translation here is the small
 * one-buffer list around the AAudio buffer. No allocation or blocking occurs
 * on the real-time callback path.
 */
#include <AudioToolbox/AudioToolbox.h>
#include <aaudio/AAudio.h>
#include <stdlib.h>
#include <string.h>

struct AudioUnitImpl {
    AAudioStream *stream;
    AudioStreamBasicDescription format;
    AURenderCallback callback;
    void *refcon;
    int initialized;
};

struct AudioComponentImpl { int output; };

static aaudio_data_callback_result_t dm_aaudio_callback(AAudioStream *stream,
                                                         void *userData,
                                                         void *audioData,
                                                         int32_t numFrames) {
    (void)stream;
    AudioUnit a = (AudioUnit)userData;
    if (!a || !a->callback || !audioData || numFrames <= 0) return AAUDIO_CALLBACK_RESULT_CONTINUE;

    AudioBufferList bl;
    memset(&bl, 0, sizeof bl);
    bl.mNumberBuffers = 1;
    bl.mBuffers[0].mNumberChannels = a->format.mChannelsPerFrame;
    bl.mBuffers[0].mDataByteSize = (UInt32)((uint64_t)numFrames * a->format.mBytesPerFrame);
    bl.mBuffers[0].mData = audioData;

    AudioTimeStamp ts;
    memset(&ts, 0, sizeof ts);
    OSStatus rc = a->callback(a->refcon, NULL, &ts, 0, (UInt32)numFrames, &bl);
    return rc == noErr ? AAUDIO_CALLBACK_RESULT_CONTINUE : AAUDIO_CALLBACK_RESULT_STOP;
}

AudioComponent AudioComponentFindNext(AudioComponent in, const AudioComponentDescription *desc) {
    (void)in;
    if (!desc || desc->componentType != kAudioUnitType_Output) return NULL;
    static struct AudioComponentImpl component = { 1 };
    return &component;
}

OSStatus AudioComponentInstanceNew(AudioComponent component, AudioUnit *out) {
    if (!component || !out) return -1;
    AudioUnit a = (AudioUnit)calloc(1, sizeof *a);
    if (!a) return -1;
    a->format.mSampleRate = 48000.0;
    a->format.mFormatID = kAudioFormatLinearPCM;
    a->format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    a->format.mChannelsPerFrame = 2;
    a->format.mBytesPerFrame = 4;
    a->format.mFramesPerPacket = 1;
    a->format.mBytesPerPacket = 4;
    a->format.mBitsPerChannel = 16;
    *out = a;
    return noErr;
}

OSStatus AudioComponentInstanceDispose(AudioUnit a) {
    if (!a) return noErr;
    if (a->stream) {
        AAudioStream_close(a->stream);
        a->stream = NULL;
    }
    free(a);
    return noErr;
}

OSStatus AudioUnitSetProperty(AudioUnit a, UInt32 property, UInt32 scope, UInt32 element,
                              const void *data, UInt32 size) {
    (void)scope; (void)element;
    if (!a || !data) return -1;
    if (property == kAudioUnitProperty_StreamFormat) {
        if (size < sizeof(AudioStreamBasicDescription)) return -1;
        memcpy(&a->format, data, sizeof a->format);
        return noErr;
    }
    if (property == kAudioUnitProperty_SetRenderCallback) {
        if (size < sizeof(AURenderCallbackStruct)) return -1;
        const AURenderCallbackStruct *cb = (const AURenderCallbackStruct *)data;
        a->callback = cb->inputProc;
        a->refcon = cb->inputProcRefCon;
        return noErr;
    }
    return noErr;
}

OSStatus AudioUnitGetProperty(AudioUnit a, UInt32 property, UInt32 scope, UInt32 element,
                              void *data, UInt32 *size) {
    (void)a; (void)property; (void)scope; (void)element; (void)data; (void)size;
    return -1;
}

OSStatus AudioUnitInitialize(AudioUnit a) {
    if (!a || !a->callback) return -1;
    AAudioStreamBuilder *b = NULL;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK || !b) return -1;
    AAudioStreamBuilder_setDirection(b, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSampleRate(b, (int32_t)a->format.mSampleRate);
    AAudioStreamBuilder_setChannelCount(b, (int32_t)a->format.mChannelsPerFrame);
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setDataCallback(b, dm_aaudio_callback, a);
    AAudioStreamBuilder_setFramesPerDataCallback(b, 192);
    aaudio_result_t rc = AAudioStreamBuilder_openStream(b, &a->stream);
    AAudioStreamBuilder_delete(b);
    if (rc != AAUDIO_OK || !a->stream) return -1;
    a->initialized = 1;
    return noErr;
}

OSStatus AudioUnitUninitialize(AudioUnit a) {
    if (!a) return -1;
    if (a->stream) {
        AAudioStream_requestStop(a->stream);
        AAudioStream_close(a->stream);
        a->stream = NULL;
    }
    a->initialized = 0;
    return noErr;
}

OSStatus AudioOutputUnitStart(AudioUnit a) {
    if (!a || !a->stream) return -1;
    return AAudioStream_requestStart(a->stream) == AAUDIO_OK ? noErr : -1;
}

OSStatus AudioOutputUnitStop(AudioUnit a) {
    if (!a || !a->stream) return -1;
    return AAudioStream_requestStop(a->stream) == AAUDIO_OK ? noErr : -1;
}

OSStatus AudioObjectGetPropertyData(AudioObjectID object, const AudioObjectPropertyAddress *address,
                                    UInt32 qualifierSize, const void *qualifierData,
                                    UInt32 *dataSize, void *data) {
    (void)object; (void)address; (void)qualifierSize; (void)qualifierData;
    if (data && dataSize && *dataSize >= sizeof(AudioObjectID)) {
        *(AudioObjectID *)data = 0;
        return noErr;
    }
    return -1;
}

int CFStringGetCString(CFStringRef string, char *buffer, CFIndex maxSize, uint32_t encoding) {
    (void)string; (void)encoding;
    if (!buffer || maxSize <= 0) return 0;
    buffer[0] = '\0';
    return 0;
}

void CFRelease(CFStringRef object) { (void)object; }
