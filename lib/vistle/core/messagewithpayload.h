#ifndef VISTLE_CORE_MESSAGEWITHPAYLOAD_H
#define VISTLE_CORE_MESSAGEWITHPAYLOAD_H

#include <vistle/core/message.h>
#include <vistle/core/messagepayload.h>

namespace vistle {

struct MessageWithPayload {
    MessageWithPayload(const vistle::message::Message &msg, const vistle::MessagePayload &payload)
    : buf(msg), payload(payload)
    {}

    explicit MessageWithPayload(const vistle::message::Message &msg): buf(msg) {}

    const char *getPayload() const
    {
        auto pl = buf.getPayload();
        if (buf.payloadSize() > 0 && !pl) {
            assert(payload);
            return payload->data();
        }
        return pl;
    }

    vistle::message::Buffer buf;
    vistle::MessagePayload payload;
};

} // namespace vistle

#endif
