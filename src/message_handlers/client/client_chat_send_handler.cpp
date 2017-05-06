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

#include "client_chat_send_handler.h"
#include <macros.h>
#include <messages/user_access_control/login_response_message.h>
#include <messages/chat/chat_send_message.h>
#include <easylogging++.h>

using namespace std;
using namespace roa;

client_chat_send_handler::client_chat_send_handler(Config config,
                                                        shared_ptr<ikafka_producer<false>> producer)
    : _config(config), _producer(producer) {

}

void client_chat_send_handler::handle_message(unique_ptr<binary_message const> const &msg,
                                                   STD_OPTIONAL<reference_wrapper<user_connection>> connection) {
    if(!connection) {
        LOG(ERROR) << NAMEOF(client_chat_send_handler::handle_message) << " received empty connection";
        return;
    }

    if(connection->get().state != user_connection_state::LOGGED_IN) {
        LOG(DEBUG) << NAMEOF(client_chat_send_handler::handle_message) << " not logged in.";
        json_login_response_message response{{false, 0, 0, 0}, 0, -1, "Need to login."};
        auto response_str = response.serialize();
        connection->get().ws->send(response_str.c_str(), response_str.length(), uWS::OpCode::TEXT);
        return;
    }

    if (auto message = dynamic_cast<binary_chat_send_message const *>(msg.get())) {
        LOG(DEBUG) << NAMEOF(client_chat_send_handler::handle_message) << " Got chat_send message from wss";
        this->_producer->enqueue_message("chat_messages", binary_chat_send_message {
                {
                        false,
                        connection->get().id,
                        _config.server_id,
                        0 // ANY
                },
                connection->get().username,
                message->target,
                message->message
        });
    } else {
        LOG(ERROR) << NAMEOF(client_chat_send_handler::handle_message) << " Couldn't cast message to chat_send_message";
        json_login_response_message response{{false, 0, 0, 0}, 0, -1, "Something went wrong."};
        auto response_str = response.serialize();
        connection->get().ws->send(response_str.c_str(), response_str.length(), uWS::OpCode::TEXT);
    }
}

uint32_t constexpr client_chat_send_handler::message_id;
