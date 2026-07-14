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
    MessagePayload(const message::Message &msg, const char *data,
                   size_t size); // non-owning payload to be immediatly sent

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

private:
    ShmVector<char> m_shmPayload;
    vistle::buffer m_payload;
    const char *m_internalPayload = nullptr;
    const char *m_borrowedPayload = nullptr;
    message::Buffer m_buffer;
};
} // namespace vistle
#endif
