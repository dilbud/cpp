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

class StreamEchoVisitor : public owt::quic::WebTransportStreamInterface::Visitor {
 public:
  explicit StreamEchoVisitor(owt::quic::WebTransportStreamInterface* stream) : stream_(stream) {

  }
  void OnCanWrite() override {}
  void OnCanRead() override {
      auto read_size = stream_->ReadableBytes();
      if (read_size == 0)
      {
          return;
      }
      std::vector<uint8_t> data(read_size);
      stream_->Read(data.data(), read_size);
      stream_->Write(data.data(), read_size);
  }
  void OnFinRead() override {}

 private:
  owt::quic::WebTransportStreamInterface* stream_;
};


class SessionEchoVisitor : public owt::quic::WebTransportSessionInterface::Visitor {
 public:
  explicit SessionEchoVisitor(owt::quic::WebTransportSessionInterface* si) : si_(si) {

  }
  ~SessionEchoVisitor() {

  }

  void OnCanCreateNewOutgoingStream(bool) override {}
  void OnConnectionClosed() override {}
  void OnIncomingStream(owt::quic::WebTransportStreamInterface *stream) override {
      std::unique_ptr<StreamEchoVisitor> visitor =
          std::make_unique<StreamEchoVisitor>(stream);
      stream->SetVisitor(visitor.get());
      stream_visitors_.push_back(std::move(visitor));
  }
  void OnDatagramReceived(const uint8_t* data, size_t length) override {
    si_->SendOrQueueDatagram((unsigned char*)data, length);
  }

 private:
  std::vector<std::unique_ptr<StreamEchoVisitor>> stream_visitors_;
  owt::quic::WebTransportSessionInterface* si_; 
};

class ServerEchoVisitor
    : public owt::quic::WebTransportServerInterface::Visitor {
 private:
  std::vector<owt::quic::WebTransportSessionInterface *> sessions_;

 public:
  explicit ServerEchoVisitor() {
        std::printf("sssssssssssssssss");
  }
  ~ServerEchoVisitor() {
        std::printf("kkkkkkkkkkkkkkkkk");
  }

  void OnEnded() override {
        std::printf("yyyyyyyyyyyyyyyyyyyyyyyyyy");
  }
  void OnSession(owt::quic::WebTransportSessionInterface *session) override {
      SessionEchoVisitor *visitor = new SessionEchoVisitor(session);
      session->SetVisitor(visitor);
      sessions_.emplace_back(session);
  }

  std::vector<owt::quic::WebTransportSessionInterface *> Sessions() const {
      return sessions_;
  }
};

int main(int, char **)
{

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
    while (true)
    {
       std::printf("session size: %lu", sv->Sessions().size());
    }
    

    return 0;
}
