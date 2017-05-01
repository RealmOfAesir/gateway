/*
    Realm of Aesir backend
    Copyright (C) 2016  Michael de Lang

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <easylogging++.h>
#include <uWS.h>
#include <json.hpp>
#include <kafka_consumer.h>
#include <kafka_producer.h>
#include <admin_messages/quit_message.h>

#include <signal.h>
#include <string>
#include <fstream>
#include <streambuf>
#include <vector>
#include <thread>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <exceptions.h>
#include <roa_di.h>
#include <macros.h>
#include "src/message_handlers/gateway/gateway_login_response_handler.h"
#include "src/message_handlers/gateway/gateway_register_response_handler.h"
#include "src/message_handlers/gateway/gateway_quit_handler.h"
#include "message_handlers/client/client_admin_quit_handler.h"
#include "message_handlers/client/client_login_handler.h"
#include "message_handlers/client/client_register_handler.h"
#include "user_connection.h"
#include "config.h"

using namespace std;
using namespace roa;

#ifdef EXPERIMENTAL_OPTIONAL
using namespace experimental;
#endif

using json = nlohmann::json;


INITIALIZE_EASYLOGGINGPP

atomic<bool> quit{false};
atomic<bool> uwsQuit{false};
mutex connectionMutex;

void on_sigint(int sig) {
    quit = true;
}

void init_extras() noexcept {
    ios::sync_with_stdio(false);
    signal(SIGINT, on_sigint);
}

void init_logger(Config const config) noexcept {
    el::Configurations defaultConf;
    defaultConf.setGlobally(el::ConfigurationType::Format, "%datetime %level: %msg");
    if(!config.debug_level.empty()) {
        if(config.debug_level == "error") {
            defaultConf.set(el::Level::Warning, el::ConfigurationType::Enabled, "false");
            defaultConf.set(el::Level::Info, el::ConfigurationType::Enabled, "false");
            defaultConf.set(el::Level::Debug, el::ConfigurationType::Enabled, "false");
            defaultConf.set(el::Level::Trace, el::ConfigurationType::Enabled, "false");
        } else if(config.debug_level == "warning") {
            defaultConf.set(el::Level::Info, el::ConfigurationType::Enabled, "false");
            defaultConf.set(el::Level::Debug, el::ConfigurationType::Enabled, "false");
            defaultConf.set(el::Level::Trace, el::ConfigurationType::Enabled, "false");
        } else if(config.debug_level == "info") {
            defaultConf.set(el::Level::Debug, el::ConfigurationType::Enabled, "false");
            defaultConf.set(el::Level::Trace, el::ConfigurationType::Enabled, "false");
        } else if(config.debug_level == "debug") {
            defaultConf.set(el::Level::Trace, el::ConfigurationType::Enabled, "false");
        }
    }
    el::Loggers::reconfigureAllLoggers(defaultConf);
    LOG(INFO) << "debug level: " << config.debug_level;
}



Config parse_env_file() {
    string env_contents;
    ifstream env(".env");

    if(!env) {
        LOG(ERROR) << "[main] no .env file found. Please make one.";
        exit(1);
    }

    env.seekg(0, ios::end);
    env_contents.resize(env.tellg());
    env.seekg(0, ios::beg);
    env.read(&env_contents[0], env_contents.size());
    env.close();

    auto env_json = json::parse(env_contents);
    Config config;

    try {
        config.broker_list = env_json["BROKER_LIST"];
    } catch (const std::exception& e) {
        LOG(ERROR) << "[main] BROKER_LIST missing in .env file.";
        exit(1);
    }

    try {
        config.group_id = env_json["GROUP_ID"];
    } catch (const std::exception& e) {
        LOG(ERROR) << "[main] GROUP_ID missing in .env file.";
        exit(1);
    }

    try {
        config.server_id = env_json["SERVER_ID"];
    } catch (const std::exception& e) {
        LOG(ERROR) << "[main] SERVER_ID missing in .env file.";
        exit(1);
    }

    if(config.server_id == 0) {
        LOG(ERROR) << "[main] SERVER_ID has to be greater than 0";
        exit(1);
    }

    try {
        config.connection_string = env_json["CONNECTION_STRING"];
    } catch (const std::exception& e) {
        LOG(ERROR) << "[main] CONNECTION_STRING missing in .env file.";
        exit(1);
    }

    try {
        config.debug_level = env_json["DEBUG_LEVEL"];
    } catch (const std::exception& e) {
        LOG(ERROR) << "[main] DEBUG_LEVEL missing in .env file.";
        exit(1);
    }

    return config;
}

unique_ptr<thread> create_uws_thread(Config config, uWS::Hub &h, shared_ptr<ikafka_producer<false>> producer, unordered_map<string, user_connection> &connections) {
    if(!producer) {
        LOG(ERROR) << "[main:backend] one of the arguments are null";
        throw runtime_error("[main:backend] one of the arguments are null");
    }

    return make_unique<thread>([=, &h, &connections]{
        try {
            message_dispatcher<false> client_msg_dispatcher;

            client_msg_dispatcher.register_handler<client_admin_quit_handler>(config, producer);
            client_msg_dispatcher.register_handler<client_login_handler>(config, producer, connections);
            client_msg_dispatcher.register_handler<client_register_handler>(config, producer, connections);

            h.onMessage([&](uWS::WebSocket<uWS::SERVER> *ws, char *recv_msg, size_t length, uWS::OpCode opCode) {
                LOG(DEBUG) << "[main:uws] Got message from wss";
                if(opCode == uWS::OpCode::TEXT) {
                    LOG(INFO) << "[main:uws] Got message from wss";
                    string str(recv_msg, length);
                    LOG(DEBUG) << str;
                    unique_lock<mutex> lock(connectionMutex);

                    string key = user_connection::AddressToString(ws->getAddress());
                    auto connection = connections.find(key);

                    if(unlikely(connection == end(connections))) {
                        LOG(ERROR) << "[main:uws] got message from " << key << " without connection";
                        ws->terminate();
                        return;
                    }

                    try {
                        auto msg = message<true>::deserialize<false>(str);
                        if (get<1>(msg)) {
                            client_msg_dispatcher.trigger_handler(msg, make_optional(ref(connection->second)));
                        }
                    } catch(const std::exception& e) {
                        LOG(ERROR) << "[main:uws] exception when deserializing message, disconnecting " << connection->second.state
                                   << ":" << connection->second.username << ":exception: " << typeid(e).name() << "-" << e.what();

                        connections.erase(key);
                        ws->terminate();
                    }
                } else {
                    ws->send(recv_msg, length, opCode);
                }
            });

            h.onConnection([&connections](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest request) {
                LOG(WARNING) << "[main:uws] Got a connection";
                string key = user_connection::AddressToString(ws->getAddress());
                unique_lock<mutex> lock(connectionMutex);
                if(connections.find(key) != end(connections)) {
                    LOG(WARNING) << "[main:uws] Connection already present, closing this one";
                    ws->terminate();
                    return;
                }
                connections.insert(make_pair(key, user_connection(ws)));
            });

            h.onDisconnection([&connections](uWS::WebSocket<uWS::SERVER> *ws, int code, char *message, size_t length) {

                string key = user_connection::AddressToString(ws->getAddress());
                unique_lock<mutex> lock(connectionMutex);
                connections.erase(key);

                LOG(WARNING) << "[main:uws] Got a disconnect, " << connections.size() << " connections remaining";
            });

            h.onError([](int type) {
                LOG(WARNING) << "[main:uws] Got error:" << type;
            });

            //auto context = uS::TLS::createContext("cert.pem", "key.pem", "test");
            //h.getDefaultGroup<uWS::SERVER>().addAsync();
            if(!h.listen(3000/*, nullptr, 0, group.get()*/)) {
                LOG(ERROR) << "[main:uws] h.listen failed";
                return;
            }

            LOG(INFO) << "[main:uws] starting create_uws_thread";

            h.run();

            uwsQuit = true;
        } catch (const runtime_error& e) {
            LOG(ERROR) << "[main:uws] error: " << typeid(e).name() << "-" << e.what();
        }
    });
}

int main() {
    Config config;
    try {
        config = parse_env_file();
    } catch (const std::exception& e) {
        LOG(ERROR) << "[main] .env file is malformed json.";
        exit(1);
    }

    init_logger(config);
    init_extras();

    auto common_injector = create_common_di_injector();

    auto producer = common_injector.create<shared_ptr<ikafka_producer<false>>>();
    auto gateway_consumer = common_injector.create<unique_ptr<ikafka_consumer<false>>>();
    gateway_consumer->start(config.broker_list, config.group_id, std::vector<std::string>{"server-" + to_string(config.server_id), "broadcast"});
    producer->start(config.broker_list);
    unordered_map<string, user_connection> connections;

    uWS::Hub h;

    try {
        auto uws_thread = create_uws_thread(config, h, producer, connections);
        message_dispatcher<false> server_gateway_msg_dispatcher;

        server_gateway_msg_dispatcher.register_handler<gateway_quit_handler>(&quit);
        server_gateway_msg_dispatcher.register_handler<gateway_login_response_handler>(config, connections);
        server_gateway_msg_dispatcher.register_handler<gateway_register_response_handler>(config, connections);

        LOG(INFO) << "[main] starting main thread";

        while (!quit) {
            try {
                producer->poll(10);
                auto msg = gateway_consumer->try_get_message(10);
                if (get<1>(msg)) {
                    unique_lock<mutex> lock(connectionMutex);
                    LOG(INFO) << "[main] Got message from kafka";

                    auto id = get<1>(msg)->sender.client_id;
                    auto connection = find_if(begin(connections), end(connections), [id](auto &t) {
                        return get<1>(t).id == id;
                    });

                    if (connection == end(connections)) {
                        LOG(DEBUG) << "[main] Got message for client_id " << id << " but no connection found";
                        continue;
                    }

                    server_gateway_msg_dispatcher.trigger_handler(msg, make_optional(ref(get<1>(*connection))));
                }
            } catch (serialization_exception &e) {
                cout << "[main] received exception " << e.what() << endl;
            }
        }

        LOG(INFO) << "[main] closing";

        auto loop = h.getLoop();
        auto closeLambda = [](Async *as) -> void {
            uWS::Hub *hub = static_cast<uWS::Hub *>(as->data);
            hub->getLoop()->destroy();
        };
        Async async{loop};
        async.setData(&h);
        async.start(closeLambda);
        async.send();

        producer->close();
        gateway_consumer->close();

        auto now = chrono::system_clock::now().time_since_epoch().count();
        auto wait_until = (chrono::system_clock::now() += 2000ms).time_since_epoch().count();

        while (!uwsQuit && now < wait_until) {
            this_thread::sleep_for(100ms);
            now = chrono::system_clock::now().time_since_epoch().count();
        }

        async.close();

        if(!uwsQuit) {
            uws_thread->detach();
        } else {
            uws_thread->join();
        }
    } catch (const runtime_error& e) {
        LOG(ERROR) << "[main] error: " << typeid(e).name() << "-" << e.what();
    }

    return 0;
}
