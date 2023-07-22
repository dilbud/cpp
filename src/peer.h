#include <memory>

#include "uv.h"

class Peer {
   public:
    Peer(const uv_loop_t* loop);

    void periodic();

    ~Peer();

   private:
    std::unique_ptr<uv_timer_t> m_timer;
};