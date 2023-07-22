#include "Peer.h"

#include <memory>

Peer::Peer(const uv_loop_t* loop) : m_timer(std::make_unique<uv_timer_t>()) {
    // Initialize the timer.
    uv_timer_init(const_cast<uv_loop_t*>(loop), m_timer.get());

    // Set up data pointer.
    uv_handle_set_data((uv_handle_t*)(m_timer.get()), this);

    // Start it.
    uv_timer_start(
        m_timer.get(),
        [](uv_timer_t* timer) {
            // Call the method.
            auto self = (Peer*)uv_handle_get_data((uv_handle_t*)timer);
            self->periodic();
        },
        // Repeat once a second.
        1000, 1000);
}

void Peer::periodic() { std::printf("dddddddddddddddd\n"); }

Peer::~Peer() {
    // Destructor
    uv_timer_stop(m_timer.get());

    // Release ownership.
    auto handle = m_timer.release();
    uv_close((uv_handle_t*)handle, [](uv_handle_t* handle) { delete handle; });
}