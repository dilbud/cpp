#include <stdio.h>
#include <iostream>
#include <string>

#include <memory>
#include <vector>

#include "owt/quic/export.h"
#include "owt/quic/logging.h"
#include "owt/quic/version.h"
#include "owt/quic/web_transport_factory.h"
#include "owt/quic/web_transport_server_interface.h"
#include "owt/quic/web_transport_session_interface.h"
#include "owt/quic/web_transport_definitions.h"
#include "owt/quic/web_transport_stream_interface.h"



class StreamEchoVisitor
    : public owt::quic::WebTransportStreamInterface::Visitor {
   public:
    explicit StreamEchoVisitor(owt::quic::WebTransportSessionInterface *session,
                               owt::quic::WebTransportStreamInterface *stream)
        : session_(session), stream_(stream) {}
    void OnCanWrite() override { std::printf("stream can write \n"); }
    void OnCanRead() override {
        std::printf("stream can read \n");
        std::printf("stream ID %d \n", stream_->Id());
        std::printf("stream WRITABLE %d \n", stream_->CanWrite());
        if (stream_->CanWrite()) {
            std::string sstr= std::string("hello world from server");
            std::vector<uint8_t> vec(sstr.begin(), sstr.end());
            size_t rs = stream_->Write(vec.data(), sstr.size());
            std::printf("stream write size %ld \n", rs);
        }
        std::printf("stream readable BYTE %ld \n", stream_->ReadableBytes());
        std::printf("stream Buffer B_BYTE %ld \n", stream_->BufferedDataBytes());
        size_t read_size = stream_->ReadableBytes();
        if (read_size > 0) {
            std::vector<uint8_t> data(read_size);
            stream_->Read(data.data(), read_size);
            std::string str;
            str.assign(data.data(), data.data() + read_size);
            std::printf("received msg: %s \n", str.c_str());
            std::printf("received msg length: %lu\n", str.size());
        }
    }
    void OnFinRead() override { std::printf("FIN READ \n"); }

   private:
    owt::quic::WebTransportSessionInterface *session_;
    owt::quic::WebTransportStreamInterface *stream_;
};

class SessionEchoVisitor : public owt::quic::WebTransportSessionInterface::Visitor {
 public:
  explicit SessionEchoVisitor(owt::quic::WebTransportSessionInterface *session)
      : session_(session) {}
  ~SessionEchoVisitor() {}

  void OnCanCreateNewOutgoingStream(bool ) override {
    std::printf("can create outgoing stream \n");
  }
  void OnConnectionClosed() override {
    std::printf("Con Closed \n");
  }
  void OnIncomingStream(owt::quic::WebTransportStreamInterface *stream) override {
      std::printf("\nstart...............OnIncomingStream\n\n");
      std::printf("stream creating\n");
      std::printf("stream ID %d \n", stream->Id());
      StreamEchoVisitor* visitor = new StreamEchoVisitor(session_, stream);
      stream->SetVisitor(visitor);
      stream_visitors_.emplace_back(visitor);
      std::printf("\nend...............OnIncomingStream\n\n");
  }

  void OnDatagramReceived(const uint8_t *data, size_t length) override {
      std::printf("\nstart...............OnDatagramReceived\n\n");
      std::printf("session ID %s \n", session_->ConnectionId());
      std::string str;
      str.assign(data, data + length);
      std::printf("received msg: %s \n",  str.c_str());
      std::printf("received msg length: %lu\n", length);
      owt::quic::MessageStatus ms =
          session_->SendOrQueueDatagram((unsigned char *)data, length);
      using namespace owt::quic;
      std::printf("msg send \n");
      switch (ms) {
            case MessageStatus::kSuccess:
            std::printf("Success\n");
            break;
            case MessageStatus::kEncryptionNotEstablished:
            std::printf("Failed to send message because encryption is not established yet\n");
            break;
            case MessageStatus::kUnsupported:
            std::printf("Failed to send message because MESSAGE frame is not supported by the connection\n");
            break;
            case MessageStatus::kBlocked:
            std::printf("Failed to send message because connection is congestion control blocked or underlying socket is write blocked\n");
            break;
            case MessageStatus::kTooLarge:
            std::printf("Failed to send message because the message is too large to fit into a single packet\n");
            break;
            case MessageStatus::kInternalError:
            std::printf("Failed to send message because connection reaches an invalid state\n");
            break;
            case MessageStatus::kUnavailable:
            std::printf("Message status is not available. When C++17 std::optional is enabled, this value will be removed\n");
            break;
            default:
            std::printf("Unknown\n");
      }
      const char* ch = "dddddddddddd";
      owt::quic::WebTransportStreamInterface * bistream = session_->CreateBidirectionalStream();
      StreamEchoVisitor* visitor = new StreamEchoVisitor(session_, bistream);
      bistream->SetVisitor(visitor);
      std::printf("\nend...............OnDatagramReceived\n\n");
  }

 private:
  std::vector<StreamEchoVisitor *> stream_visitors_;
  owt::quic::WebTransportSessionInterface* session_; 
};

class ServerEchoVisitor
    : public owt::quic::WebTransportServerInterface::Visitor {
 private:
  std::vector<SessionEchoVisitor *> sessionVisitors_;

 public:
  explicit ServerEchoVisitor() {
  }
  ~ServerEchoVisitor() {
  }

  void OnEnded() override {
        std::printf("Session end\n");
  }
  void OnSession(owt::quic::WebTransportSessionInterface *session) override {
      std::printf("\nstart...............OnSession\n\n");
      std::printf("session creating\n");
      std::printf("session ID %s \n", session->ConnectionId());
      SessionEchoVisitor *visitor = new SessionEchoVisitor(session);
      session->SetVisitor(visitor);
      sessionVisitors_.emplace_back(visitor);

    //   while(true) {
    //   uint64_t i = session->GetStats().estimated_bandwidth;
    //   std::printf("xxxxxxxxxxxxxxx %lu \n", i);
    //   }
    //   if (session->IsSessionReady()) {
    //     //   std::printf("zzzzzzzzzzzzzzzzzzzzzzzz0\n");
    //     //   std::string s = std::string("sgfsdfsdf");
    //     //   owt::quic::MessageStatus ms = session->SendOrQueueDatagram(
    //     //       (unsigned char *)s.c_str(), s.size());
    //     //       printf("My enum Value sssssssssssssssss\n");
    //     //   printf("My enum Value : %d\n", (int)ms);

    //     while (true)
    //     {
    //       std::string s = std::string("sgfsdfsdf");
    //       owt::quic::MessageStatus ms = session->SendOrQueueDatagram(
    //           (unsigned char *)s.c_str(), s.size());
    //           printf("My enum Value sssssssssssssssss\n");
    //       printf("My enum Value : %d\n", (int)ms);
    //     }
        
    //   }
    std::printf("\nend...............OnSession\n\n");
  }

  std::vector<SessionEchoVisitor *> SessionVisitors() const {
      return sessionVisitors_;
  }
};

void quicServer() {
    owt::quic::Logging::Severity(owt::quic::LoggingSeverity::kInfo);
    owt::quic::Logging::InitLogging();

    owt::quic::Version v;
	std::string a = v.VersionNumber();
	std::printf("Version Number: %s\n", a.c_str());

	owt::quic::WebTransportFactory *webTFactory = owt::quic::WebTransportFactory::Create();

	owt::quic::WebTransportServerInterface *webTServerInter = 
        webTFactory->CreateWebTransportServer(
            8500, 
            "/workspaces/cpp/cert/out/leaf_cert.p12", 
            "hello");
    ServerEchoVisitor *sv = new ServerEchoVisitor();
    if (webTServerInter) {
        webTServerInter->SetVisitor(sv);
        int a = webTServerInter->Start();
        std::printf("server running %d\n", a);
    } else {
        std::printf("server terminated\n");
    }
    bool isc = false;
    while (true) {
        // std::printf("session size: %lu\n", sv->SessionVisitors().size());
        // if (sv->Sessions().size() > 0) {
        //   if (!isc) {
        //       std::string s = std::string("sgfsdfsdf");
        //       owt::quic::WebTransportStreamInterface *bis =
        //           sv->Sessions().front()->CreateBidirectionalStream();
        //       if (bis->CanWrite()) {
        //           bis->Write((unsigned char *)s.c_str(), s.size());
        //         //   std::printf("write %ld\n", rs);
        //       }
        //       isc = true;
        //   }
        // }
    }
}

int main(int, char **) {
    quicServer();
    return 0;
}
