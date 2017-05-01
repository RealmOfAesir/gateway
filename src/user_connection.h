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

#pragma once

#include <uWS.h>
#include <string>
#include <atomic>

namespace roa {
    enum user_connection_state {
        UNKNOWN,
        REGISTERING,
        LOGGED_IN
    };

    struct user_connection {
        user_connection_state state;
        int8_t admin_status;
        uWS::WebSocket<uWS::SERVER> *ws;
        int64_t id;
        std::string username;
        static std::atomic<int64_t> idCounter;

        explicit user_connection(uWS::WebSocket<uWS::SERVER> *ws);
        user_connection(user_connection const &conn);

        static std::string AddressToString(uS::Socket::Address &&a);
    };
}