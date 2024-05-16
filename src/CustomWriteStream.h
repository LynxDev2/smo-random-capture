//Credits: Anethyst-szs

#include <sead/stream/seadStream.h>
#include <sead/stream/seadRamStream.h>

class CustomWriteStream : public sead::WriteStream {
public:
    CustomWriteStream(sead::RamStreamSrc* src, sead::Stream::Modes mode)
    {
        mSrc = src;
        setMode(mode);
    }
};
