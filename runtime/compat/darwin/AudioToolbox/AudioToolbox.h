/*
 * AudioToolbox compatibility declarations for Android.
 *
 * The upstream HLE audio queue is portable; only its Apple AudioUnit sink is
 * platform-specific. These declarations preserve that queue and map the sink
 * to an AAudio stream in dm_compat_audio.c.
 */
#ifndef DM_COMPAT_AUDIOTOOLBOX_H
#define DM_COMPAT_AUDIOTOOLBOX_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t OSStatus;
typedef uint32_t UInt32;
typedef uint64_t UInt64;
typedef uint32_t AudioUnitRenderActionFlags;
typedef uint32_t AudioObjectID;
typedef long CFIndex;
typedef const void *CFStringRef;
typedef uint32_t AudioDeviceID;

#define noErr 0
#define kAudioFormatLinearPCM 0x6c70636d
#define kLinearPCMFormatFlagIsSignedInteger (1u << 2)
#define kLinearPCMFormatFlagIsPacked (1u << 3)
#define kAudioUnitType_Output 0x61756f75
#define kAudioUnitSubType_DefaultOutput 0x64656661
#define kAudioUnitSubType_RemoteIO 0x72696f20
#define kAudioUnitManufacturer_Apple 0x6170706c
#define kAudioUnitProperty_StreamFormat 8
#define kAudioUnitProperty_SetRenderCallback 23
#define kAudioUnitScope_Global 0
#define kAudioUnitScope_Input 1
#define kAudioOutputUnitProperty_CurrentDevice 2000
#define kAudioObjectPropertyName  kAudioObjectPropertyScopeGlobal
#define kAudioObjectPropertyScopeGlobal 0
#define kAudioObjectPropertyElementMain 0
#define kAudioHardwarePropertyDefaultOutputDevice 2
#define kAudioObjectSystemObject 1
#define kCFStringEncodingUTF8 0x08000100u

typedef struct {
    UInt32 componentType;
    UInt32 componentSubType;
    UInt32 componentManufacturer;
    UInt32 componentFlags;
    UInt32 componentFlagsMask;
} AudioComponentDescription;

typedef struct {
    double mSampleRate;
    UInt32 mFormatID;
    UInt32 mFormatFlags;
    UInt32 mBytesPerPacket;
    UInt32 mFramesPerPacket;
    UInt32 mBytesPerFrame;
    UInt32 mChannelsPerFrame;
    UInt32 mBitsPerChannel;
    UInt32 mReserved;
} AudioStreamBasicDescription;

typedef struct { uint64_t mDummy[2]; } AudioTimeStamp;
typedef struct {
    UInt32 mNumberBuffers;
    struct { UInt32 mNumberChannels; UInt32 mDataByteSize; void *mData; } mBuffers[1];
} AudioBufferList;

typedef struct AudioUnitImpl *AudioUnit;
typedef struct AudioComponentImpl *AudioComponent;
typedef OSStatus (*AURenderCallback)(void *, AudioUnitRenderActionFlags *, const AudioTimeStamp *, UInt32, UInt32, AudioBufferList *);
typedef struct {
    AURenderCallback inputProc;
    void *inputProcRefCon;
} AURenderCallbackStruct;

typedef struct { UInt32 mSelector; UInt32 mScope; UInt32 mElement; } AudioObjectPropertyAddress;
typedef struct { UInt32 componentType; } AudioComponentInstance;

AudioComponent AudioComponentFindNext(AudioComponent, const AudioComponentDescription *);
OSStatus AudioComponentInstanceNew(AudioComponent, AudioUnit *);
OSStatus AudioComponentInstanceDispose(AudioUnit);
OSStatus AudioUnitSetProperty(AudioUnit, UInt32, UInt32, UInt32, const void *, UInt32);
OSStatus AudioUnitGetProperty(AudioUnit, UInt32, UInt32, UInt32, void *, UInt32 *);
OSStatus AudioUnitInitialize(AudioUnit);
OSStatus AudioUnitUninitialize(AudioUnit);
OSStatus AudioOutputUnitStart(AudioUnit);
OSStatus AudioOutputUnitStop(AudioUnit);
OSStatus AudioObjectGetPropertyData(AudioObjectID, const AudioObjectPropertyAddress *, UInt32, const void *, UInt32 *, void *);
int CFStringGetCString(CFStringRef, char *, CFIndex, uint32_t);
void CFRelease(CFStringRef);

#ifdef __cplusplus
}
#endif

#endif /* DM_COMPAT_AUDIOTOOLBOX_H */
