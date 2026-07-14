#include "messagepayload.h"
#include "message.h"
namespace vistle {

MessagePayload::MessagePayload(const message::Message &msg): m_buffer(msg)
{}

MessagePayload::MessagePayload(const message::Message &msg, const vistle::buffer &payload, bool shm): m_buffer(msg)
{
    if (!(m_internalPayload = m_buffer.addPayload(&payload)))
        if (shm)
            m_shmPayload = ShmVector<char>(payload.data(), payload.size());
        else
            m_payload = payload;
}

MessagePayload::MessagePayload(const message::Message &msg, const char *data, size_t size)
: m_buffer(msg), m_borrowedPayload(data)
{
    m_buffer.setPayloadSize(size);
}

MessagePayload::MessagePayload(const MessagePayload &other)
: m_shmPayload(other.m_shmPayload)
, m_payload(other.m_payload)
, m_borrowedPayload(other.m_borrowedPayload)
, m_buffer(other.m_buffer)
{
    m_internalPayload = m_buffer.getPayload();
}

MessagePayload::MessagePayload(MessagePayload &&other)
: m_shmPayload(std::move(other.m_shmPayload))
, m_payload(std::move(other.m_payload))
, m_borrowedPayload(other.m_borrowedPayload)
, m_buffer(std::move(other.m_buffer))
{
    m_internalPayload = m_buffer.getPayload();
}

MessagePayload &MessagePayload::operator=(const MessagePayload &other)
{
    if (this != &other) {
        m_shmPayload = other.m_shmPayload;
        m_payload = other.m_payload;
        m_borrowedPayload = other.m_borrowedPayload;
        m_buffer = other.m_buffer;
        m_internalPayload = m_buffer.getPayload();
    }
    return *this;
}

MessagePayload &MessagePayload::operator=(MessagePayload &&other)
{
    if (this != &other) {
        m_shmPayload = std::move(other.m_shmPayload);
        m_payload = std::move(other.m_payload);
        m_borrowedPayload = other.m_borrowedPayload;
        m_buffer = std::move(other.m_buffer);
        m_internalPayload = m_buffer.getPayload();
    }
    return *this;
}

void MessagePayload::updatePayload()
{
    if (size() == 0)
        return;
    if (!(m_internalPayload = m_buffer.getPayload()))
        m_shmPayload = Shm::the().getArrayFromName<char>(m_buffer.payloadName());
}

const char *MessagePayload::data() const
{
    if (m_buffer.payloadSize() == 0) {
        return nullptr;
    }
    if (m_internalPayload) {
        return m_internalPayload;
    }
    if (m_payload.size() > 0) {
        return m_payload.data();
    }
    if (m_borrowedPayload) {
        return m_borrowedPayload;
    }
    assert(m_shmPayload);
    return m_shmPayload->data();
}

const char *MessagePayload::begin() const
{
    return data();
}

const char *MessagePayload::end() const
{
    return begin() + size();
}

size_t MessagePayload::size() const
{
    return m_buffer.payloadSize();
}

message::Buffer &MessagePayload::buffer()
{
    return m_buffer;
}

const message::Buffer &MessagePayload::buffer() const
{
    return m_buffer;
}


} // namespace vistle
