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

#include "client_login_handler.h"
#include <macros.h>
#include <easylogging++.h>
#include <messages/user_access_control/login_message.h>
#include <messages/user_access_control/login_response_message.h>

using namespace std;
using namespace roa;

client_login_handler::client_login_handler(Config config,
                                           shared_ptr<ikafka_producer<false>> producer)
    : _config(config), _producer(producer) {

}

void client_login_handler::handle_message(unique_ptr<binary_message const> const &msg, STD_OPTIONAL<std::reference_wrapper<user_connection>> connection) {
    if(!connection) {
        LOG(ERROR) << NAMEOF(client_login_handler::handle_message) << " received empty connection";
        return;
    }

    if(connection->get().state != user_connection_state::UNKNOWN) {
        LOG(DEBUG) << NAMEOF(client_login_handler::handle_message) << " Got register message from wss while not in unknown connection state";
        json_login_response_message response{{false, 0, 0, 0}, 0, -1, "Already logged in or awaiting response on register request."};
        auto response_str = response.serialize();
        connection->get().ws->send(response_str.c_str(), response_str.length(), uWS::OpCode::TEXT);
        return;
    }

    if (auto message = dynamic_cast<binary_login_message const *>(msg.get())) {
        LOG(DEBUG) << NAMEOF(client_login_handler::handle_message) << " Got login message from wss";
        connection->get().username = message->username;
        this->_producer->enqueue_message("user_access_control_messages", binary_login_message {
                {
                    false,
                    connection->get().id,
                    _config.server_id,
                    0 // ANY
                },
                message->username,
                message->password,
                connection->get().ws->getAddress().address
        });
    } else {
        LOG(ERROR) << NAMEOF(client_login_handler::handle_message) << " Couldn't cast message to login_message";
        json_login_response_message response{{false, 0, 0, 0}, 0, -1, "Something went wrong."};
        auto response_str = response.serialize();
        connection->get().ws->send(response_str.c_str(), response_str.length(), uWS::OpCode::TEXT);
    }
}

uint32_t constexpr client_login_handler::message_id;