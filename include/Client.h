#ifndef MOSQUITTO_CPP_LIBRARY_H
#define MOSQUITTO_CPP_LIBRARY_H

#include <atomic>
#include <mutex>

struct mosquitto;
struct mosquitto_message;

namespace MQTT
{
    class Client
    {
      public:
        class ClientListener
        {
          public:
            virtual void OnConnect(const bool& success, const std::string& reason)                                = 0;
            virtual void OnPublish(int mid)                                                                       = 0;
            virtual void OnDisconnect(int mid)                                                                    = 0;
            virtual void OnLog(const std::string& message)                                                        = 0;
            virtual void OnMessage(const std::string& topic, const int& qos, const void* payload, const int& len) = 0;
            virtual void OnSubscribe(const int& mid, const bool& have_subscription)                               = 0;
            virtual void OnUnsubscribe(int mid)                                                                   = 0;
            virtual void OnError(const std::string& error)                                                        = 0;
        };

        Client(ClientListener* m_client_listener);

        virtual ~Client();
        bool Init(const char* cafile_path, const char* cert_path, const char* key_path, const bool& debug = false);
        bool Connect(const char* host, const int& port, const int& timeout);
        bool Publish(const void* payload, const int& payload_len, int& message_id) const;
        bool Subscribe(const std::string& topic, int qos, int& message_id);
        bool IsConnected() const;
        void SetDebug(const bool& debug);

      private:
        mosquitto*         m_mosq;
        mutable std::mutex m_mosq_lk;
        std::atomic<bool>  m_connected;
        std::atomic<bool>  m_debug;
        ClientListener*    m_client_listener;

        static void on_connect(mosquitto* mosq, void* obj, int reason_code);
        static void on_publish(mosquitto* mosq, void* obj, int mid);
        static void on_disconnect(mosquitto* mosq, void* obj, int mid);
        static void on_log(mosquitto* mosq, void* obj, int, const char* message);
        static void on_message(mosquitto*, void* obj, const mosquitto_message*);
        static void on_subscribe(struct mosquitto*, void* obj, int mid, int qos, const int* granted_qos);
        static void on_unsubscribe(struct mosquitto* mosq, void* obj, int mid);
    };
} // namespace MQTT

#endif // MOSQUITTO_CPP_LIBRARY_H
