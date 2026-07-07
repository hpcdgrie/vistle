#ifndef VISTLE_CORE_ENVELOPE_H
#define VISTLE_CORE_ENVELOPE_H

#include "export.h"
#include <vector>
#include "message.h"
#include <vistle/util/buffer.h>
namespace vistle {

namespace message {
class V_COREEXPORT Envelope {
public:
    Envelope() = default; //set buffer() manually and use updatePayload() to eventually get shm/internal payload
    Envelope(const message::Message &msg); // MessagePayload without payload
    Envelope(const message::Buffer &msg); // overload for Buffer so that it can be copied with its full payload

    virtual ~Envelope() = default;
    virtual std::unique_ptr<Envelope> clone() const;

    const char *data() const;
    const char *begin() const;
    const char *end() const;

    size_t payloadSize() const; //payload size
    size_t headerSize() const; // header + eventual internal payload size
    size_t externalPayloadSize() const; //
    void updateHeader();

    template<typename SomeMessage>
    SomeMessage &as()
    {
        return m_header.as<SomeMessage>();
    }

    template<typename SomeMessage>
    const SomeMessage &as() const
    {
        return m_header.as<SomeMessage>();
    }

    message::Buffer &header();
    const message::Buffer &header() const;

    // save a copy if we already have a payload as buffer
    // todo: change the archive functions so that they can work on other arrays
    template<typename Payload>
    Payload getPayload() const
    {
        assert(payloadSize() > 0);
        return message::getPayload<Payload>({begin(), end()});
    }
    bool internalPayload() const;


protected:
    Envelope(const Envelope &other);
    Envelope &operator=(const Envelope &other);
    Envelope(const message::Message &msg, const char *payload, size_t payloadSize);

    virtual const char *payloadData() const;

    // keep m_buffer first
    message::Buffer m_header;
    const char *m_internalPayload = nullptr;
};

class V_COREEXPORT BufferEnvelope: public Envelope {
public:
    using Envelope::Envelope;
    BufferEnvelope(const message::Message &msg, const vistle::buffer &payload); // copy payload to buffer
    BufferEnvelope(const message::Message &msg,
                   std::shared_ptr<vistle::buffer> payload); // use existing buffer as payload
    std::unique_ptr<Envelope> clone() const override;
    const std::shared_ptr<vistle::buffer> &payload() const;
    std::shared_ptr<vistle::buffer> &payload();

private:
    const char *payloadData() const override;
    std::shared_ptr<vistle::buffer> m_payload;
};

} // namespace message
} // namespace vistle
#endif
