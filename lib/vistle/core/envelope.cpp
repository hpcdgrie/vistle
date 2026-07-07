#include "envelope.h"
#include "message.h"

namespace vistle {
namespace message {

Envelope::Envelope(const message::Message &msg): m_header(msg)
{
    assert(msg.payloadSize() == 0);
}

Envelope::Envelope(const message::Message &msg, const char *payload, size_t payloadSize): m_header(msg)
{
    m_internalPayload = m_header.addPayload(payload, payloadSize);
    m_header.setPayloadSize(payloadSize);
}

Envelope::Envelope(const message::Buffer &msg): m_header(msg), m_internalPayload(m_header.getPayload())
{}

std::unique_ptr<Envelope> Envelope::clone() const
{
    return std::make_unique<Envelope>(this->m_header);
}

Envelope::Envelope(const Envelope &other): m_header(other.m_header)
{
    m_internalPayload = m_header.getPayload();
}

Envelope &Envelope::operator=(const Envelope &other)
{
    if (this != &other) {
        m_header = other.m_header;
        m_internalPayload = m_header.getPayload();
    }
    return *this;
}

void Envelope::updateHeader()
{
    if (m_header.payloadSize() == 0)
        return;
    m_internalPayload = m_header.getPayload();
}

const char *Envelope::data() const
{
    if (m_header.payloadSize() == 0) {
        return nullptr;
    }
    assert(m_internalPayload || payloadData());

    return m_internalPayload ? m_internalPayload : payloadData();
}

const char *Envelope::begin() const
{
    return data();
}

const char *Envelope::end() const
{
    return begin() + payloadSize();
}

size_t Envelope::headerSize() const
{
    return m_header.size() + (m_internalPayload ? m_header.payloadSize() : 0);
}

size_t Envelope::externalPayloadSize() const
{
    return m_internalPayload ? 0 : payloadSize();
}

message::Buffer &Envelope::header()
{
    return m_header;
}

const message::Buffer &Envelope::header() const
{
    return m_header;
}

bool Envelope::internalPayload() const
{
    return m_internalPayload;
}

size_t Envelope::payloadSize() const
{
    assert(m_header.payloadSize() == 0 || m_internalPayload || payloadData());
    return m_header.payloadSize();
}

const char *Envelope::payloadData() const
{
    return m_internalPayload;
}


BufferEnvelope::BufferEnvelope(const message::Message &msg, const vistle::buffer &payload)
: Envelope(msg, payload.data(), payload.size())
{
    if (!m_internalPayload) {
        m_payload = std::make_shared<vistle::buffer>(payload);
        m_header.setPayloadSize(payload.size());
    }
}

BufferEnvelope::BufferEnvelope(const message::Message &msg, std::shared_ptr<vistle::buffer> payload)
: Envelope(msg, payload->data(), payload->size())
{
    if (!m_internalPayload) {
        m_payload = payload;
        m_header.setPayloadSize(payload->size());
    }
}

std::unique_ptr<Envelope> BufferEnvelope::clone() const
{
    return std::make_unique<BufferEnvelope>(*this);
}

const char *BufferEnvelope::payloadData() const
{
    return m_payload ? m_payload->data() : nullptr;
}

const std::shared_ptr<vistle::buffer> &BufferEnvelope::payload() const
{
    return m_payload;
}

std::shared_ptr<vistle::buffer> &BufferEnvelope::payload()
{
    return m_payload;
}

} // namespace message
} // namespace vistle
