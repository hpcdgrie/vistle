#ifndef VISTLE_CORE_MESSAGEPAYLOAD_H
#define VISTLE_CORE_MESSAGEPAYLOAD_H

#include "shm.h"
#include "export.h"
#include <vector>
#include "message.h"
#include <vistle/util/buffer.h>
namespace vistle {

class V_COREEXPORT MessagePayload {
public:
    MessagePayload() = default; //set buffer() manually and use updatePayload() to eventually get shm/internal payload
    MessagePayload(const message::Message &msg); // MessagePayload without payload
    MessagePayload(const message::Message &msg, const vistle::buffer &payload,
                   bool shm = true); // copy payload to shm or buffer
    MessagePayload(const message::Message &msg,
                   std::shared_ptr<vistle::buffer> payload); // use existing buffer as payload

    MessagePayload(const MessagePayload &other);
    MessagePayload(MessagePayload &&other);

    MessagePayload &operator=(const MessagePayload &other);
    MessagePayload &operator=(MessagePayload &&other);

    const char *data() const;
    const char *begin() const;
    const char *end() const;
    size_t size() const;
    void updatePayload();

    template<typename SomeMessage>
    SomeMessage &as()
    {
        return m_buffer.as<SomeMessage>();
    }

    template<typename SomeMessage>
    const SomeMessage &as() const
    {
        return m_buffer.as<SomeMessage>();
    }

    message::Buffer &buffer();
    const message::Buffer &buffer() const;
    vistle::buffer *payload();

    // save a copy if we alredy have a payload as buffer
    // todo: change the archive functions so that they can work on other arrays
    template<typename Payload>
    Payload getPayload() const
    {
        if (m_payload)
            return message::getPayload<Payload>(*m_payload);
        else
            return message::getPayload<Payload>({begin(), end()});
    }

private:
    ShmVector<char> m_shmPayload;
    std::shared_ptr<vistle::buffer> m_payload;
    const char *m_internalPayload = nullptr;
    message::Buffer m_buffer;
};
} // namespace vistle
#endif
