/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/node/sessions/session_inbound.hpp>

namespace libbitcoin {
namespace node {

// Inbound connection attempts are dropped unless confirmed chain is current.
// Used instead of suspension because suspension has independent start/stop.
bool session_inbound::enabled() const NOEXCEPT
{
    return !config().node.delay_inbound || is_recent();
}

} // namespace node
} // namespace libbitcoin
